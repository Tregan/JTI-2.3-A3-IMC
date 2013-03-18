#ifndef _MENU_H
#define _MENU_H

#ifdef __cplusplus
extern "C" {
#endif
    
//Code goes here
//Functions for Menu Actions Here
void ShowSetting(void);

//Standard Function for Menu.
void DisplayItem(char text[], int);
//Disable the Menu
void menuExit(void);
void MenuInit(void);

#ifdef __cplusplus
}
#endif

#endif
