//
//  library_scene.c
//  CrankBoy
//
//  Created by Matteo D'Ignazio on 15/05/22.
//  Maintained and developed by the CrankBoy dev team.
//

#include "library_scene.h"

#include "app.h"
#include "dtcm.h"
#include "game_scene.h"
#include "minigb_apu.h"
#include "modal.h"
#include "preferences.h"
#include "settings_scene.h"

static void PGB_LibraryScene_update(void *object, float dt);
static void PGB_LibraryScene_free(void *object);
static void PGB_LibraryScene_reloadList(PGB_LibraryScene *libraryScene);
static void PGB_LibraryScene_menu(void *object);

PGB_LibraryScene *PGB_LibraryScene_new(void)
{
    playdate->system->setCrankSoundsDisabled(false);

    PGB_Scene *scene = PGB_Scene_new();

    PGB_LibraryScene *libraryScene = pgb_malloc(sizeof(PGB_LibraryScene));
    libraryScene->scene = scene;

    DTCM_VERIFY_DEBUG();

    scene->managedObject = libraryScene;

    scene->update = PGB_LibraryScene_update;
    scene->free = PGB_LibraryScene_free;
    scene->menu = PGB_LibraryScene_menu;

    libraryScene->model =
        (PGB_LibrarySceneModel){.empty = true, .tab = PGB_LibrarySceneTabList};

    libraryScene->games = array_new();
    libraryScene->listView = PGB_ListView_new();
    libraryScene->tab = PGB_LibrarySceneTabList;
    libraryScene->lastSelectedItem = -1;

    libraryScene->missingCoverIcon = NULL;

    DTCM_VERIFY_DEBUG();

    PGB_LibraryScene_reloadList(libraryScene);

    return libraryScene;
}

static void PGB_LibraryScene_listFiles(const char *filename, void *userdata)
{
    PGB_LibraryScene *libraryScene = userdata;

    char *extension;
    char *dot = pgb_strrchr(filename, '.');

    if (!dot || dot == filename)
    {
        extension = "";
    }
    else
    {
        extension = dot + 1;
    }

    if ((pgb_strcmp(extension, "gb") == 0 || pgb_strcmp(extension, "gbc") == 0))
    {
        PGB_Game *game = PGB_Game_new(filename);
        array_push(libraryScene->games, game);
    }
}

static void PGB_LibraryScene_reloadList(PGB_LibraryScene *libraryScene)
{
    for (int i = 0; i < libraryScene->games->length; i++)
    {
        PGB_Game *game = libraryScene->games->items[i];
        PGB_Game_free(game);
    }

    array_clear(libraryScene->games);

    DTCM_VERIFY();

    playdate->file->listfiles(PGB_gamesPath, PGB_LibraryScene_listFiles,
                              libraryScene, 0);

    DTCM_VERIFY();
    pgb_sort_games_array(libraryScene->games);
    DTCM_VERIFY_DEBUG();

    PGB_Array *items = libraryScene->listView->items;

    for (int i = 0; i < items->length; i++)
    {
        PGB_ListItem *item = items->items[i];
        PGB_ListItem_free(item);
    }

    array_clear(items);

    for (int i = 0; i < libraryScene->games->length; i++)
    {
        PGB_Game *game = libraryScene->games->items[i];

        PGB_ListItemButton *itemButton =
            PGB_ListItemButton_new(game->displayName);
        array_push(items, itemButton->item);
    }

    if (items->length > 0)
    {
        libraryScene->tab = PGB_LibrarySceneTabList;
    }
    else
    {
        libraryScene->tab = PGB_LibrarySceneTabEmpty;
    }

    DTCM_VERIFY_DEBUG();

    PGB_ListView_reload(libraryScene->listView);
}

static void PGB_LibraryScene_update(void *object, float dt)
{
    PGB_LibraryScene *libraryScene = object;

    PGB_Scene_update(libraryScene->scene, dt);

    PDButtons pressed = PGB_App->buttons_pressed;

    if (pressed & kButtonA)
    {
        int selectedItem = libraryScene->listView->selectedItem;
        if (selectedItem >= 0 &&
            selectedItem < libraryScene->listView->items->length)
        {

            PGB_Game *game = libraryScene->games->items[selectedItem];

            save_test("a");
            save_test("a2");
            save_test("a3");

            PGB_GameScene *gameScene = PGB_GameScene_new(game->fullpath);
            if (gameScene)
            {
                PGB_present(gameScene->scene);
            }

            playdate->system->logToConsole("Present gameScene");
        }
    }

    bool needsDisplay = false;

    if (libraryScene->model.empty ||
        libraryScene->model.tab != libraryScene->tab ||
        libraryScene->scene->forceFullRefresh)
    {
        needsDisplay = true;
        if (libraryScene->scene->forceFullRefresh)
        {
            libraryScene->scene->forceFullRefresh = false;
        }
    }

    libraryScene->model.empty = false;
    libraryScene->model.tab = libraryScene->tab;

    if (needsDisplay)
    {
        playdate->graphics->clear(kColorWhite);
    }

    if (libraryScene->tab == PGB_LibrarySceneTabList)
    {
        int screenWidth = playdate->display->getWidth();
        int screenHeight = playdate->display->getHeight();

        int rightPanelWidth = 241;
        int leftPanelWidth = screenWidth - rightPanelWidth;

        libraryScene->listView->needsDisplay = needsDisplay;
        libraryScene->listView->frame =
            PDRectMake(0, 0, leftPanelWidth, screenHeight);

        PGB_ListView_update(libraryScene->listView);
        PGB_ListView_draw(libraryScene->listView);

        int selectedIndex = libraryScene->listView->selectedItem;

        bool selectionChanged =
            (selectedIndex != libraryScene->lastSelectedItem);

        if (needsDisplay || libraryScene->listView->needsDisplay ||
            selectionChanged)
        {
            libraryScene->lastSelectedItem = selectedIndex;

            playdate->graphics->fillRect(leftPanelWidth + 1, 0,
                                         rightPanelWidth - 1, screenHeight,
                                         kColorWhite);

            if (selectedIndex >= 0 &&
                selectedIndex < libraryScene->games->length)
            {
                PGB_Game *selectedGame =
                    libraryScene->games->items[selectedIndex];

                if (selectedGame->coverPath != NULL)
                {
                    PGB_LoadedCoverArt cover_art =
                        pgb_load_and_scale_cover_art_from_path(
                            selectedGame->coverPath, 240, 240);

                    if (cover_art.status == PGB_COVER_ART_SUCCESS &&
                        cover_art.bitmap != NULL)
                    {
                        int panel_content_width = rightPanelWidth - 1;
                        int coverX =
                            leftPanelWidth + 1 +
                            (panel_content_width - cover_art.scaled_width) / 2;
                        int coverY =
                            (screenHeight - cover_art.scaled_height) / 2;

                        playdate->graphics->setDrawMode(kDrawModeCopy);
                        playdate->graphics->drawBitmap(
                            cover_art.bitmap, coverX, coverY, kBitmapUnflipped);
                    }
                    else
                    {
                        const char *message = "Error";
                        if (cover_art.status == PGB_COVER_ART_FILE_NOT_FOUND)
                        {
                            message = "Cover not found";
                            playdate->system->logToConsole(
                                "Cover %s not found by load func.",
                                selectedGame->coverPath);
                        }
                        else if (cover_art.status ==
                                 PGB_COVER_ART_ERROR_LOADING)
                        {
                            message = "Error loading image";
                        }
                        else if (cover_art.status ==
                                 PGB_COVER_ART_INVALID_IMAGE)
                        {
                            message = "Invalid image";
                        }

                        playdate->graphics->setFont(PGB_App->bodyFont);
                        int textWidth = playdate->graphics->getTextWidth(
                            PGB_App->bodyFont, message, pgb_strlen(message),
                            kUTF8Encoding, 0);
                        int panel_content_width = rightPanelWidth - 1;
                        int textX = leftPanelWidth + 1 +
                                    (panel_content_width - textWidth) / 2;
                        int textY =
                            (screenHeight - playdate->graphics->getFontHeight(
                                                PGB_App->bodyFont)) /
                            2;

                        playdate->graphics->setDrawMode(kDrawModeFillBlack);
                        playdate->graphics->drawText(
                            message, pgb_strlen(message), kUTF8Encoding, textX,
                            textY);
                    }
                    pgb_free_loaded_cover_art_bitmap(&cover_art);
                }
                else
                {
                    if (libraryScene->missingCoverIcon == NULL)
                    {
                        libraryScene->missingCoverIcon =
                            playdate->graphics->loadBitmap("launcher/icon",
                                                           NULL);
                    }

                    LCDBitmap *iconBitmap = libraryScene->missingCoverIcon;

                    static const char *title = "Missing cover";
                    static const char *message1 = "Connect to a computer";
                    static const char *message2 = "and copy covers to:";
                    static const char *message3 = "Data/*.crankboy/covers";

                    LCDFont *titleFont = PGB_App->bodyFont;
                    LCDFont *bodyFont = PGB_App->subheadFont;

                    int imageToTitleSpacing = 8;
                    int titleToMessageSpacing = 6;
                    int messageLineSpacing = 2;

                    int iconWidth = 0, iconHeight = 0;
                    if (iconBitmap)
                    {
                        playdate->graphics->getBitmapData(
                            iconBitmap, &iconWidth, &iconHeight, NULL, NULL,
                            NULL);
                    }

                    int titleHeight =
                        playdate->graphics->getFontHeight(titleFont);
                    int messageHeight =
                        playdate->graphics->getFontHeight(bodyFont);

                    int containerHeight =
                        (iconHeight > 0 ? iconHeight + imageToTitleSpacing
                                        : 0) +
                        titleHeight + titleToMessageSpacing +
                        (messageHeight * 3) + (messageLineSpacing * 2);
                    int containerY_start = (screenHeight - containerHeight) / 2;

                    int panel_content_width = rightPanelWidth - 1;

                    int iconX = leftPanelWidth + 1 +
                                (panel_content_width - iconWidth) / 2;
                    int titleX = leftPanelWidth + 1 +
                                 (panel_content_width -
                                  playdate->graphics->getTextWidth(
                                      titleFont, title, pgb_strlen(title),
                                      kUTF8Encoding, 0)) /
                                     2;
                    int message1_X =
                        leftPanelWidth + 1 +
                        (panel_content_width -
                         playdate->graphics->getTextWidth(bodyFont, message1,
                                                          pgb_strlen(message1),
                                                          kUTF8Encoding, 0)) /
                            2;
                    int message2_X =
                        leftPanelWidth + 1 +
                        (panel_content_width -
                         playdate->graphics->getTextWidth(bodyFont, message2,
                                                          pgb_strlen(message2),
                                                          kUTF8Encoding, 0)) /
                            2;
                    int message3_X =
                        leftPanelWidth + 1 +
                        (panel_content_width -
                         playdate->graphics->getTextWidth(bodyFont, message3,
                                                          pgb_strlen(message3),
                                                          kUTF8Encoding, 0)) /
                            2;

                    int currentY = containerY_start;

                    int iconY = currentY;
                    if (iconBitmap)
                    {
                        currentY += iconHeight + imageToTitleSpacing;
                    }

                    int titleY = currentY;
                    currentY += titleHeight + titleToMessageSpacing;

                    int message1_Y = currentY;
                    currentY += messageHeight + messageLineSpacing;

                    int message2_Y = currentY;
                    currentY += messageHeight + messageLineSpacing;

                    int message3_Y = currentY;

                    playdate->graphics->setDrawMode(kDrawModeCopy);
                    if (iconBitmap)
                    {
                        playdate->graphics->drawBitmap(iconBitmap, iconX, iconY,
                                                       kBitmapUnflipped);
                    }

                    playdate->graphics->setDrawMode(kDrawModeFillBlack);
                    playdate->graphics->setFont(titleFont);
                    playdate->graphics->drawText(title, pgb_strlen(title),
                                                 kUTF8Encoding, titleX, titleY);

                    playdate->graphics->setFont(bodyFont);
                    playdate->graphics->drawText(message1, pgb_strlen(message1),
                                                 kUTF8Encoding, message1_X,
                                                 message1_Y);
                    playdate->graphics->drawText(message2, pgb_strlen(message2),
                                                 kUTF8Encoding, message2_X,
                                                 message2_Y);
                    playdate->graphics->drawText(message3, pgb_strlen(message3),
                                                 kUTF8Encoding, message3_X,
                                                 message3_Y);
                }
            }
            playdate->graphics->drawLine(leftPanelWidth, 0, leftPanelWidth,
                                         screenHeight, 1, kColorBlack);
        }
    }
    else if (libraryScene->tab == PGB_LibrarySceneTabEmpty)
    {
        if (needsDisplay)
        {
            static const char *title = "CrankBoy";
            static const char *message1 = "To add games:";

            static const char *message2_num = "1.";
            static const char *message2_text = "Connect to a computer";

            static const char *message3_num = "2.";
            static const char *message3_text1 = "Then hold ";
            static const char *message3_text2 = "LEFT + MENU + POWER";

            static const char *message4_num = "3.";
            static const char *message4_text1 = "Copy games to ";
            static const char *message4_text2 = "Data/*.crankboy/games";

            playdate->graphics->clear(kColorWhite);

            int titleToMessageSpacing = 8;
            int messageLineSpacing = 4;
            int verticalOffset = 2;
            int textPartSpacing = 5;

            int titleHeight =
                playdate->graphics->getFontHeight(PGB_App->titleFont);
            int subheadHeight =
                playdate->graphics->getFontHeight(PGB_App->subheadFont);
            int messageHeight =
                playdate->graphics->getFontHeight(PGB_App->bodyFont);
            int compositeLineHeight =
                (subheadHeight + verticalOffset > messageHeight)
                    ? (subheadHeight + verticalOffset)
                    : messageHeight;

            int numWidth1 = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message2_num, strlen(message2_num),
                kUTF8Encoding, 0);
            int numWidth2 = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message3_num, strlen(message3_num),
                kUTF8Encoding, 0);
            int numWidth3 = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message4_num, strlen(message4_num),
                kUTF8Encoding, 0);
            int maxNumWidth = (numWidth1 > numWidth2) ? numWidth1 : numWidth2;
            maxNumWidth = (numWidth3 > maxNumWidth) ? numWidth3 : maxNumWidth;

            int textWidth4_part1 = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message4_text1, strlen(message4_text1),
                kUTF8Encoding, 0);
            int textWidth4_part2 = playdate->graphics->getTextWidth(
                PGB_App->subheadFont, message4_text2, strlen(message4_text2),
                kUTF8Encoding, 0);
            int totalInstructionWidth = maxNumWidth + 4 + textWidth4_part1 +
                                        textPartSpacing + textWidth4_part2;

            int titleX = (playdate->display->getWidth() -
                          playdate->graphics->getTextWidth(PGB_App->titleFont,
                                                           title, strlen(title),
                                                           kUTF8Encoding, 0)) /
                         2;
            int blockAnchorX =
                (playdate->display->getWidth() - totalInstructionWidth) / 2;
            int numColX = blockAnchorX;
            int textColX = blockAnchorX + maxNumWidth + 4;

            int containerHeight =
                titleHeight + titleToMessageSpacing + messageHeight +
                messageLineSpacing + messageHeight + messageLineSpacing +
                compositeLineHeight + messageLineSpacing + compositeLineHeight;
            int titleY = (playdate->display->getHeight() - containerHeight) / 2;

            int message1_Y = titleY + titleHeight + titleToMessageSpacing;
            int message2_Y = message1_Y + messageHeight + messageLineSpacing;
            int message3_Y = message2_Y + messageHeight + messageLineSpacing;
            int message4_Y =
                message3_Y + compositeLineHeight + messageLineSpacing;

            playdate->graphics->setFont(PGB_App->titleFont);
            playdate->graphics->drawText(title, strlen(title), kUTF8Encoding,
                                         titleX, titleY);

            playdate->graphics->setFont(PGB_App->bodyFont);
            playdate->graphics->drawText(message1, strlen(message1),
                                         kUTF8Encoding, blockAnchorX,
                                         message1_Y);

            playdate->graphics->drawText(message2_num, strlen(message2_num),
                                         kUTF8Encoding, numColX, message2_Y);
            playdate->graphics->drawText(message2_text, strlen(message2_text),
                                         kUTF8Encoding, textColX, message2_Y);

            playdate->graphics->drawText(message3_num, strlen(message3_num),
                                         kUTF8Encoding, numColX, message3_Y);
            playdate->graphics->drawText(message3_text1, strlen(message3_text1),
                                         kUTF8Encoding, textColX, message3_Y);
            playdate->graphics->setFont(PGB_App->subheadFont);
            int message3_text1_width = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message3_text1, strlen(message3_text1),
                kUTF8Encoding, 0);
            playdate->graphics->drawText(
                message3_text2, strlen(message3_text2), kUTF8Encoding,
                textColX + message3_text1_width + textPartSpacing,
                message3_Y + verticalOffset);

            playdate->graphics->setFont(PGB_App->bodyFont);
            playdate->graphics->drawText(message4_num, strlen(message4_num),
                                         kUTF8Encoding, numColX, message4_Y);
            playdate->graphics->drawText(message4_text1, strlen(message4_text1),
                                         kUTF8Encoding, textColX, message4_Y);
            playdate->graphics->setFont(PGB_App->subheadFont);
            int message4_text1_width = playdate->graphics->getTextWidth(
                PGB_App->bodyFont, message4_text1, strlen(message4_text1),
                kUTF8Encoding, 0);
            playdate->graphics->drawText(
                message4_text2, strlen(message4_text2), kUTF8Encoding,
                textColX + message4_text1_width + textPartSpacing,
                message4_Y + verticalOffset);
        }
    }
}

static void PGB_LibraryScene_showSettings(void *userdata)
{
    PGB_SettingsScene *settingsScene = PGB_SettingsScene_new(NULL);
    PGB_presentModal(settingsScene->scene);
}

static void PGB_LibraryScene_menu(void *object)
{
    playdate->system->addMenuItem("Settings", PGB_LibraryScene_showSettings,
                                  object);
}

static void PGB_LibraryScene_free(void *object)
{
    PGB_LibraryScene *libraryScene = object;

    if (libraryScene->missingCoverIcon)
    {
        playdate->graphics->freeBitmap(libraryScene->missingCoverIcon);
    }

    PGB_Scene_free(libraryScene->scene);

    PGB_Array *items = libraryScene->listView->items;
    for (int i = 0; i < items->length; i++)
    {
        PGB_ListItem *item = items->items[i];
        PGB_ListItem_free(item);
    }

    for (int i = 0; i < libraryScene->games->length; i++)
    {
        PGB_Game *game = libraryScene->games->items[i];
        PGB_Game_free(game);
    }

    PGB_ListView_free(libraryScene->listView);

    array_free(libraryScene->games);

    pgb_free(libraryScene);
}

PGB_Game *PGB_Game_new(const char *filename)
{
    PGB_Game *game = pgb_malloc(sizeof(PGB_Game));
    game->filename = string_copy(filename);

    char *fullpath_str;
    playdate->system->formatString(&fullpath_str, "%s/%s", PGB_gamesPath,
                                   filename);
    game->fullpath = fullpath_str;

    char *basename_no_ext = string_copy(filename);
    char *ext = pgb_strrchr(basename_no_ext, '.');
    if (ext != NULL)
    {
        *ext = '\0';
    }

    game->displayName = string_copy(basename_no_ext);

    char *cleanName_no_ext = string_copy(basename_no_ext);
    pgb_sanitize_string_for_filename(cleanName_no_ext);

    game->coverPath =
        pgb_find_cover_art_path(basename_no_ext, cleanName_no_ext);

    if (game->coverPath)
    {
        playdate->system->logToConsole("Cover for '%s': '%s'",
                                       game->displayName, game->coverPath);
    }
    else
    {
        playdate->system->logToConsole(
            "No cover found for '%s' (basename: '%s', clean: '%s')",
            game->displayName, basename_no_ext, cleanName_no_ext);
    }

    pgb_free(basename_no_ext);
    pgb_free(cleanName_no_ext);

    return game;
}

void PGB_Game_free(PGB_Game *game)
{
    pgb_free(game->filename);
    pgb_free(game->fullpath);
    pgb_free(game->coverPath);
    pgb_free(game->displayName);

    pgb_free(game);
}
