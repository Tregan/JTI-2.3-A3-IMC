/*
 * Menu C File
 * Date:        25-02-13
 * Author:      Robin Hermans
 */

//Standard Imports
#include <stdio.h>
#include <sys/thread.h>
#include <sys/timer.h>
#include <string.h>
//Custom Imports
#include "menu.h"
#include "display.h"
#include "keyboard.h"
#include "log.h"

#define LOG_MODULE  LOG_MENU_MODULE

//START OF VARIABLES

//Ints for menuorëntations
int menu_enabled = 0;
int submenu_enabled = 0;

int menu_item = 0;
int menu_subitem = 0;

//END OF VARIABLES

//Define a struct for SubMenu Items
struct menuSubItem {
    char* text;
    void (*function)(void);;
};

/*
 * START OF MENU ACTION
 * All Functions for menu actions go in here.
 * !Make sure the function is defined in the right place in the menu.h!
 */
void ShowSetting()
{
    LogMsg_P(LOG_INFO, PSTR("ShowSettings"));
}

//END OF MENU ACTION

//!DEFINE FUNCTION ABOVE!//
char* menuitems[3] = {
    "Music",
    "Alarm",
    "Settings"
};

struct menuSubItem menusubitems[3][2] = {
    {{"Radio", &ShowSetting}, {"SD", &ShowSetting}},
    {{"Alarm 2", &ShowSetting}, {"Alarm 2", &ShowSetting}},
    {{"Time Zone", &ShowSetting}, {"IP Adress", &ShowSetting}}
};

/*
 * Thread for handling menu actions.
 * - Displays all menu items.
 */
THREAD(MenuThread, args)
{
    //Start endless loop for Thread
    for(;;)
    {
        NutSleep(100);
        u_char key = KbGetKey();
        if(key != KEY_UNDEFINED)
        {
            LcdBackLight(LCD_BACKLIGHT_ON);
            if(key == KEY_SETUP)
            {
                menu_enabled = 1;
            }
            else{
                if(key == KEY_OK)
                {
                    if(menu_enabled == 1)
                        submenu_enabled = 1;
                }
                else
                {
                    if(key == KEY_ESC)
                    {
                        if(submenu_enabled == 1)
                            submenu_enabled = 0;
                        else
                        {
                            submenu_enabled = 0;
                            menu_enabled = 0;
                        }

                    }
                }
            }
            
            //Menu Controlls
            if(menu_enabled == 1 && submenu_enabled == 0)
            {
                if(key == KEY_UP)
                {
                    if(menu_item-1 >= 0)
                    {
                        menu_item -= 1;
                    }
                }
                if(key == KEY_DOWN)
                {
                    if(menu_item+1 <= (sizeof(menuitems)/sizeof(menuitems[0]))-1)
                    {
                        menu_item += 1;
                    }
                }
            }
            if(menu_enabled == 1 && submenu_enabled == 1)
            {
                if(key == KEY_OK)
                {
                    struct menuSubItem item = menusubitems[menu_item][menu_subitem];
                    item.function();
                }
                if(key == KEY_UP)
                {
                    if(menu_subitem-1 >= 0)
                    {
                        menu_subitem -= 1;
                    }
                }
                if(key == KEY_DOWN)
                {
                    if(menu_subitem+1 <= (sizeof(menusubitems[menu_item])/sizeof(menusubitems[menu_item][0]))-1)
                    {
                        menu_subitem += 1;
                    }
                }
            }
        }
        
        //Display actions
        char* item;
        
        switch(menu_enabled)
        {
            case 0: 
                DisplayItem("", 1);
                break;
            case 1: 
                item = menuitems[menu_item];
                DisplayItem(item, 1);
                break;
        }
        switch(submenu_enabled)
        {
            case 0: 
                // Do nothing for now.
                break;
            case 1: 
                DisplayItem(menusubitems[menu_item][menu_subitem].text, 1);
                break;
        }
        while(1)
        {
            u_char key = KbGetKey();
            if(key == KEY_UNDEFINED)
                break;
        }
    }
}

void DisplayItem(char text[], int clear)
{
    if(clear == 1)
        LcdClearLine();
    
    LcdWriteSecondLine(text);
}

/*
 * Menu Initialization
 * - Starts up menu thread for display.
 */
void MenuInit()
{
    NutThreadSetPriority(1);
    NutThreadCreate("MenuThread", MenuThread, NULL, 1024);
}

