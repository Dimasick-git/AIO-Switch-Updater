#pragma once

#include <borealis.hpp>
#include <json.hpp>

#include "constants.hpp"

namespace ryazhenka { class RyazhenkaCard; }

class ListDownloadTab : public brls::List
{
private:
    // Migrated to RyazhenkaCard for the Ryazhenka visual language; the
    // outer brls::List keeps owning vertical focus + scrolling.
    ryazhenka::RyazhenkaCard* listItem;
    nlohmann::ordered_json nxlinks;
    std::string currentCheatsVer = "";
    std::string newCheatsVer = "";
    contentType type;
    void createList();
    void createList(contentType type);
    void createCheatSlipItem();
    void createGbatempItem();
    void createGfxItem();
    void setDescription();
    void setDescription(contentType type);
    void displayNotFound();

public:
    ListDownloadTab(const contentType type, const nlohmann::ordered_json& nxlinks = nlohmann::ordered_json::object());
};