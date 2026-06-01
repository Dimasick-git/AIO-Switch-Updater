#include "worker_page.hpp"

#include <functional>
#include <string>

#include "constants.hpp"
#include "download.hpp"
#include "extract.hpp"
#include "main_frame.hpp"
#include "ryazhenka_logger.hpp"
#include "progress_event.hpp"
#include "utils.hpp"

namespace i18n = brls::i18n;
using namespace i18n::literals;

WorkerPage::WorkerPage(brls::StagedAppletFrame* frame, const std::string& text, worker_func_t worker_func) : frame(frame), workerFunc(worker_func), text(text)
{
    this->progressDisp = new brls::ProgressDisplay();
    this->progressDisp->setParent(this);

    this->label = new brls::Label(brls::LabelStyle::DIALOG, text, true);
    this->label->setHorizontalAlign(NVG_ALIGN_CENTER);
    this->label->setParent(this);

    this->button = new brls::Button(brls::ButtonStyle::REGULAR);
    this->button->setParent(this);

    this->registerAction("menus/common/cancel"_i18n, brls::Key::B, [this] {
        ProgressEvent::instance().setInterupt(true);
        return true;
    });
    this->registerAction("", brls::Key::A, [this] { return true; });
    this->registerAction("", brls::Key::PLUS, [this] { return true; });
}

std::string formatLabelText(double speed, double fileSizeCurrent, double fileSizeFinal)
{
    double fileSizeCurrentMB = fileSizeCurrent / 0x100000;
    double fileSizeFinalMB = fileSizeFinal / 0x100000;
    double speedMB = speed / 0x100000;

    double timeRemaining = (fileSizeFinal - fileSizeCurrent) / speed;
    int hours = static_cast<int>(timeRemaining / 3600);
    int minutes = static_cast<int>((timeRemaining - hours * 3600) / 60);
    int seconds = static_cast<int>(timeRemaining - hours * 3600 - minutes * 60);

    std::string labelText = fmt::format("menus/worker/download_progress"_i18n, fileSizeCurrentMB, fileSizeFinalMB, speedMB);
    if (speedMB > 0) {
        std::string eta;
        if (hours > 0)
            eta += fmt::format("{}h ", hours);
        if (minutes > 0)
            eta += fmt::format("{}m ", minutes);

        eta += fmt::format("{}s", seconds);
        labelText += "\n" + fmt::format("menus/worker/time_left"_i18n, eta);
    }

    return labelText;
}

void WorkerPage::draw(NVGcontext* vg, int x, int y, unsigned width, unsigned height, brls::Style* style, brls::FrameContext* ctx)
{
    if (this->draw_page) {
        if (!this->workStarted) {
            this->workStarted = true;
            appletSetMediaPlaybackState(true);
            appletBeginBlockingHomeButton(0);
            ProgressEvent::instance().reset();
            workerThread = new std::thread(&WorkerPage::doWork, this);
        }
        else if (ProgressEvent::instance().finished()) {
            appletEndBlockingHomeButton();
            appletSetMediaPlaybackState(false);
            // statusCode == 0 means the worker either never wrote one (curl
            // failure, callsite did not propagate) or genuine no-network.
            // Treating it as failure surfaces an error instead of silently
            // showing "all done" on a 0-byte file.
            const long sc = ProgressEvent::instance().getStatusCode();
            const bool interrupted = ProgressEvent::instance().getInterupt();
            // Only a real error code (>399) is a failure. statusCode 0 means
            // "this stage reported no HTTP status" — which is the NORMAL state
            // for every non-download stage (extract, cheats merge, joy-con
            // backup, delete, …) because ProgressEvent::reset() leaves it 0 and
            // those workers only set progress steps. The old `sc == 0` failure
            // rule popped a bogus "server unavailable / timeout" right after a
            // perfectly good download+extract. Real download failures are no
            // longer 0: downloadFile() now returns 408 when curl fails, so they
            // still trip this check.
            const bool failed = (sc > 399);

            if (interrupted || failed) {
                // Bail out of the ENTIRE staged flow on cancel or failure.
                // The old code showed the error but ALSO called nextStage(),
                // so a failed download still advanced to the extract stage
                // (running extract on a missing/truncated file) and left the
                // user stranded on a dead WorkerPage whose draw_page=false
                // guard swallowed all input — a total freeze that forced a
                // reboot. Returning to a fresh MainFrame and showing the error
                // on top of it keeps the app usable.
                this->draw_page = false;
                ProgressEvent::instance().setStatusCode(0);
                ProgressEvent::instance().setInterupt(false);
                brls::Application::pushView(new MainFrame());
                if (failed)
                    brls::Application::crash(fmt::format("menus/errors/error_message"_i18n, util::getErrorMessage(sc)));
                return;
            }

            ProgressEvent::instance().setStatusCode(0);
            frame->nextStage();
        }
        else {
            this->progressDisp->setProgress(ProgressEvent::instance().getStep(), ProgressEvent::instance().getMax());
            this->progressDisp->frame(ctx);
            if (ProgressEvent::instance().getTotal()) {
                this->label->setText(formatLabelText(ProgressEvent::instance().getSpeed(), ProgressEvent::instance().getNow(), ProgressEvent::instance().getTotal()));
            }
            this->label->frame(ctx);
        }
    }
}

void WorkerPage::layout(NVGcontext* vg, brls::Style* style, brls::FontStash* stash)
{
    this->label->setWidth(roundf((float)this->width * style->CrashFrame.labelWidth));

    this->label->setBoundaries(
        this->x + this->width / 2 - this->label->getWidth() / 2,
        this->y + (this->height - style->AppletFrame.footerHeight) / 2 - 30,
        this->label->getWidth(),
        this->label->getHeight());

    this->progressDisp->setBoundaries(
        this->x + this->width / 2 - style->CrashFrame.buttonWidth,
        this->y + this->height / 2,
        style->CrashFrame.buttonWidth * 2,
        style->CrashFrame.buttonHeight);
}

void WorkerPage::doWork()
{
    // Runs on a worker thread. ANY exception escaping here would unwind into the
    // std::thread and call std::terminate → "program closed". Extraction (bad
    // zip, filesystem_error on a weird path, the cheats merge) is the usual
    // culprit. Catch everything, log it, and report a failure status so the UI
    // shows a clean error instead of crashing.
    try {
        if (this->workerFunc)
            this->workerFunc();
    } catch (const std::exception& e) {
        ryazhenka::log::error(std::string("worker stage threw: ") + e.what());
        ProgressEvent::instance().setStatusCode(500);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
    } catch (...) {
        ryazhenka::log::error("worker stage threw a non-std exception");
        ProgressEvent::instance().setStatusCode(500);
        ProgressEvent::instance().setStep(ProgressEvent::instance().getMax());
    }
}

brls::View* WorkerPage::getDefaultFocus()
{
    return this->button;
}

WorkerPage::~WorkerPage()
{
    if (this->workStarted && workerThread->joinable()) {
        this->workerThread->join();
        if (this->workerThread)
            delete this->workerThread;
    }
    delete this->progressDisp;
    delete this->label;
    delete this->button;
}