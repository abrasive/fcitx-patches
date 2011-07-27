/***************************************************************************
 *   Copyright (C) 2009~2010 by t3swing                                    *
 *   t3swing@sina.com                                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
#include <ctype.h>
#include <math.h>
#include <iconv.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "fcitx-config/fcitx-config.h"
#include "fcitx-utils/log.h"
#include <cairo-xlib.h>
#include "fcitx/ui.h"
#include "fcitx/module.h"
#include <module/x11/x11stuff.h>

#include "skin.h"
#include "classicui.h"
#include "MenuWindow.h"
#include "fcitx/instance.h"
#include <fcitx-utils/utils.h>

static boolean ReverseColor(XlibMenu * Menu,int shellIndex);
static void MenuMark(XlibMenu* menu, int y, int i);
static void DrawArrow(XlibMenu* menu, int line_y);
static void MoveSubMenu(XlibMenu *sub, XlibMenu *parent, int offseth);
static void DisplayText(XlibMenu * menu,int shellindex,int line_y);
static void DrawDivLine(XlibMenu * menu,int line_y);
static boolean MenuWindowEventHandler(void *arg, XEvent* event);
static int SelectShellIndex(XlibMenu * menu, int x, int y, int* offseth);
static void CloseAllMenuWindow(FcitxClassicUI *classicui);
static void CloseAllSubMenuWindow(XlibMenu *xlibMenu);
static void CloseOtherSubMenuWindow(XlibMenu *xlibMenu, XlibMenu* subMenu);
static boolean IsMouseInOtherMenu(XlibMenu *xlibMenu, int x, int y);
static void InitXlibMenu(XlibMenu* menu);
static void ReloadXlibMenu(void* arg, boolean enabled);

#define GetMenuShell(m, i) ((MenuShell*) utarray_eltptr(&(m)->shell, (i)))

void InitXlibMenu(XlibMenu* menu)
{
    FcitxClassicUI* classicui = menu->owner;
    char        strWindowName[]="Fcitx Menu Window";
    XSetWindowAttributes attrib;
    unsigned long   attribmask;
    int depth;
    Colormap cmap;
    Visual * vs;
    XGCValues xgv;
    GC gc;
    Display* dpy = classicui->dpy;
    int iScreen = classicui->iScreen;
    
    vs=ClassicUIFindARGBVisual (classicui);
    ClassicUIInitWindowAttribute(classicui, &vs, &cmap, &attrib, &attribmask, &depth);

    //开始只创建一个简单的窗口不做任何动作
    menu->menuWindow =XCreateWindow (dpy,
                                     RootWindow (dpy, iScreen),
                                     0, 0,
                                     MENU_WINDOW_WIDTH,MENU_WINDOW_HEIGHT,
                                     0, depth, InputOutput,
                                     vs, attribmask, &attrib);

    if (menu->menuWindow == (Window) NULL)
        return;

    XSetTransientForHint (dpy, menu->menuWindow, DefaultRootWindow (dpy));
    
    menu->pixmap = XCreatePixmap(dpy,
                                 menu->menuWindow,
                                 MENU_WINDOW_WIDTH,
                                 MENU_WINDOW_HEIGHT,
                                 depth);

    xgv.foreground = WhitePixel(dpy, iScreen);
    gc = XCreateGC(dpy, menu->pixmap, GCForeground, &xgv);
    XFillRectangle(
        dpy,
        menu->pixmap,
        gc,
        0,
        0,
        MENU_WINDOW_WIDTH,
        MENU_WINDOW_HEIGHT);
    menu->menu_cs=cairo_xlib_surface_create(dpy,
                                            menu->pixmap,
                                            vs,
                                            MENU_WINDOW_WIDTH,MENU_WINDOW_HEIGHT);
    XFreeGC(dpy, gc);

    XSelectInput (dpy, menu->menuWindow, KeyPressMask | ExposureMask | ButtonPressMask | ButtonReleaseMask  | PointerMotionMask | LeaveWindowMask | StructureNotifyMask );
    
    ClassicUISetWindowProperty(classicui, menu->menuWindow, FCITX_WINDOW_MENU, strWindowName);
    
    menu->iPosX=100;
    menu->iPosY=100;
    menu->width=cairo_image_surface_get_height(menu->menu_cs);
}


XlibMenu* CreateMainMenuWindow(FcitxClassicUI *classicui)
{
    XlibMenu* menu = CreateXlibMenu(classicui);
    menu->menushell = &classicui->mainMenu;
    
    return menu;
}

boolean MenuWindowEventHandler(void *arg, XEvent* event)
{
    XlibMenu* menu = (XlibMenu*) arg;
    if (event->xany.window == menu->menuWindow)
    {
        switch(event->type)
        {
            case MapNotify:
                UpdateMenuShell(menu->menushell);
                break;
            case Expose:
                DrawXlibMenu(menu);
                break;
            case LeaveNotify:
                {
                    int x = event->xcrossing.x_root;
                    int y = event->xcrossing.y_root;

                    if (!IsMouseInOtherMenu(menu, x, y))
                    {
                        CloseAllSubMenuWindow(menu);
                    }
                }
                break;
            case MotionNotify:
                {
                    int offseth = 0;
                    GetMenuSize(menu);
                    int i=SelectShellIndex(menu, event->xmotion.x, event->xmotion.y, &offseth);
                    boolean flag = ReverseColor(menu,i);
                    if (!flag)
                    {
                        DrawXlibMenu(menu);
                    }
                    MenuShell *shell = GetMenuShell(menu->menushell, i);
                    if (shell && shell->type == MENUTYPE_SUBMENU && shell->subMenu)
                    {
                        XlibMenu* subxlibmenu = (XlibMenu*) shell->subMenu->uipriv;
                        CloseOtherSubMenuWindow(menu, subxlibmenu);
                        MoveSubMenu(subxlibmenu, menu, offseth);
                        DrawXlibMenu(subxlibmenu);
                        XMapRaised(menu->owner->dpy, subxlibmenu->menuWindow);
                    }
                    if (shell == NULL)                        
                        CloseOtherSubMenuWindow(menu, NULL);
                }
                break;
            case ButtonPress:
                {
                    switch(event->xbutton.button)
                    {
                        case Button1:
                            {
                                int offseth;
                                int i=SelectShellIndex(menu, event->xmotion.x, event->xmotion.y, &offseth);
                                if (menu->menushell->MenuAction)
                                {
                                    if (menu->menushell->MenuAction(menu->menushell, i))
                                        CloseAllMenuWindow(menu->owner);
                                }
                            }
                            break;
                        case Button3:
                            CloseAllMenuWindow(menu->owner);
                            break;
                    }
                }
                break;
        }
        return true;
    }
    return false;
}

void CloseAllMenuWindow(FcitxClassicUI *classicui)
{
    FcitxInstance* instance = classicui->owner;
    FcitxUIMenu** menupp;
    for (menupp = (FcitxUIMenu **) utarray_front(&instance->uimenus);
        menupp != NULL;
        menupp = (FcitxUIMenu **) utarray_next(&instance->uimenus, menupp)
    )
    {
        XlibMenu* xlibMenu = (XlibMenu*) (*menupp)->uipriv;
        XUnmapWindow(classicui->dpy, xlibMenu->menuWindow);
    }
    XUnmapWindow(classicui->dpy, classicui->mainMenuWindow->menuWindow);
}

void CloseOtherSubMenuWindow(XlibMenu *xlibMenu, XlibMenu* subMenu)
{
    MenuShell *menu;
    for (menu = (MenuShell *) utarray_front(&xlibMenu->menushell->shell);
        menu != NULL;
        menu = (MenuShell *) utarray_next(&xlibMenu->menushell->shell, menu)
    )
    {
        if (menu->type == MENUTYPE_SUBMENU && menu->subMenu && menu->subMenu->uipriv != subMenu)
        {
            CloseAllSubMenuWindow((XlibMenu *)menu->subMenu->uipriv);
        }
    }
}

void CloseAllSubMenuWindow(XlibMenu *xlibMenu)
{
    MenuShell *menu;
    for (menu = (MenuShell *) utarray_front(&xlibMenu->menushell->shell);
        menu != NULL;
        menu = (MenuShell *) utarray_next(&xlibMenu->menushell->shell, menu)
    )
    {
        if (menu->type == MENUTYPE_SUBMENU && menu->subMenu)
        {
            CloseAllSubMenuWindow((XlibMenu *)menu->subMenu->uipriv);
        }
    }
    XUnmapWindow(xlibMenu->owner->dpy, xlibMenu->menuWindow);
}

boolean IsMouseInOtherMenu(XlibMenu *xlibMenu, int x, int y)
{
    FcitxClassicUI *classicui = xlibMenu->owner;
    FcitxInstance* instance = classicui->owner;
    FcitxUIMenu** menupp;
    for (menupp = (FcitxUIMenu **) utarray_front(&instance->uimenus);
        menupp != NULL;
        menupp = (FcitxUIMenu **) utarray_next(&instance->uimenus, menupp)
    )
    {
        
        XlibMenu* otherXlibMenu = (XlibMenu*) (*menupp)->uipriv;
        if (otherXlibMenu == xlibMenu)
            continue;
        XWindowAttributes attr;
        XGetWindowAttributes(classicui->dpy, otherXlibMenu->menuWindow, &attr);
        if (attr.map_state != IsUnmapped &&
            IsInBox(x, y, attr.x, attr.y, attr.width, attr.height))
        {
            return true;
        }
    }
    
    XlibMenu* otherXlibMenu = classicui->mainMenuWindow;
    if (otherXlibMenu == xlibMenu)
        return false;
    XWindowAttributes attr;
    XGetWindowAttributes(classicui->dpy, otherXlibMenu->menuWindow, &attr);
    if (attr.map_state != IsUnmapped &&
        IsInBox(x, y, attr.x, attr.y, attr.width, attr.height))
    {
        return true;
    }
    return false;
}

XlibMenu* CreateXlibMenu(FcitxClassicUI *classicui)
{
    XlibMenu *menu = fcitx_malloc0(sizeof(XlibMenu));
    menu->owner = classicui;
    InitXlibMenu(menu);

    FcitxModuleFunctionArg arg;
    arg.args[0] = MenuWindowEventHandler;
    arg.args[1] = menu;
    InvokeFunction(classicui->owner, FCITX_X11, ADDXEVENTHANDLER, arg);
    
    arg.args[0] = ReloadXlibMenu;
    arg.args[1] = menu;
    InvokeFunction(classicui->owner, FCITX_X11, ADDCOMPOSITEHANDLER, arg);
    return menu;
}

void GetMenuSize(XlibMenu * menu)
{
    int i=0;
    int winheight=0;
    int fontheight=0;
    int menuwidth = 0;
    FcitxSkin *sc = &menu->owner->skin;

    winheight = sc->skinMenu.marginTop + sc->skinMenu.marginBottom;//菜单头和尾都空8个pixel
    fontheight= sc->skinFont.menuFontSize;
    for (i=0;i<utarray_len(&menu->menushell->shell);i++)
    {
        if ( GetMenuShell(menu->menushell, i)->type == MENUTYPE_SIMPLE || GetMenuShell(menu->menushell, i)->type == MENUTYPE_SUBMENU)
            winheight += 6+fontheight;
        else if ( GetMenuShell(menu->menushell, i)->type == MENUTYPE_DIVLINE)
            winheight += 5;
        
        int width = StringWidth(GetMenuShell(menu->menushell, i)->tipstr, menu->owner->menuFont, sc->skinFont.menuFontSize);
        if (width > menuwidth)
            menuwidth = width;        
    }
    menu->height = winheight;
    menu->width = menuwidth + sc->skinMenu.marginLeft + sc->skinMenu.marginRight + 15 + 20;
}

//根据Menu内容来绘制菜单内容
void DrawXlibMenu(XlibMenu * menu)
{
    FcitxSkin *sc = &menu->owner->skin;
    FcitxClassicUI *classicui = menu->owner;
    Display* dpy = classicui->dpy;
    GC gc = XCreateGC( dpy, menu->menuWindow, 0, NULL );
    int i=0;
    int fontheight;
    int iPosY = 0;
    cairo_t* cr=cairo_create(menu->menu_cs);
    SkinImage *background = LoadImage(sc, sc->skinMenu.backImg, false);

    fontheight= sc->skinFont.menuFontSize;

    GetMenuSize(menu);

    DrawResizableBackground(cr, background->image, menu->height, menu->width,
                            sc->skinMenu.marginLeft,  sc->skinMenu.marginTop,  sc->skinMenu.marginRight,  sc->skinMenu.marginBottom);

    cairo_destroy(cr);
    
    iPosY=sc->skinMenu.marginTop;
    for (i=0;i<utarray_len(&menu->menushell->shell);i++)
    {
        if ( GetMenuShell(menu->menushell, i)->type == MENUTYPE_SIMPLE || GetMenuShell(menu->menushell, i)->type == MENUTYPE_SUBMENU)
        {
            DisplayText( menu,i,iPosY);
            if (menu->menushell->mark == i)
                MenuMark(menu,iPosY,i);
        
            if (GetMenuShell(menu->menushell, i)->type == MENUTYPE_SUBMENU)
                DrawArrow(menu, iPosY);
            iPosY=iPosY+6+fontheight;
        }
        else if ( GetMenuShell(menu->menushell, i)->type == MENUTYPE_DIVLINE)
        {
            DrawDivLine(menu,iPosY);
            iPosY+=5;
        }
    }
    
    XResizeWindow(dpy, menu->menuWindow, menu->width, menu->height);
    XCopyArea (dpy,
               menu->pixmap,
               menu->menuWindow,
               gc,
               0,
               0,
               menu->width,
               menu->height, 0, 0);
    XFreeGC(dpy, gc);
}

void DisplayXlibMenu(XlibMenu * menu)
{
    FcitxClassicUI *classicui = menu->owner;
    Display* dpy = classicui->dpy;
    XMapRaised (dpy, menu->menuWindow);
    XMoveWindow(dpy, menu->menuWindow, menu->iPosX, menu->iPosY);
}

void DrawDivLine(XlibMenu * menu,int line_y)
{
    FcitxSkin *sc = &menu->owner->skin;
    int marginLeft = sc->skinMenu.marginLeft;
    int marginRight = sc->skinMenu.marginRight;
    cairo_t * cr;
    cr=cairo_create(menu->menu_cs);
    fcitx_cairo_set_color(cr, &sc->skinMenu.lineColor);
    cairo_set_line_width (cr, 2);
    cairo_move_to(cr, marginLeft + 3, line_y+3);
    cairo_line_to(cr, menu->width - marginRight - 3, line_y+3);
    cairo_stroke(cr);
    cairo_destroy(cr);
}

void MenuMark(XlibMenu * menu,int y,int i)
{
    FcitxSkin *sc = &menu->owner->skin;
    int marginLeft = sc->skinMenu.marginLeft;
    double size = (sc->skinFont.menuFontSize * 0.7 ) / 2;
    cairo_t *cr;
    cr = cairo_create(menu->menu_cs);
    if (GetMenuShell(menu->menushell, i)->isselect == 0)
    {
        fcitx_cairo_set_color(cr, &sc->skinFont.menuFontColor[MENU_INACTIVE]);
    }
    else
    {
        fcitx_cairo_set_color(cr, &sc->skinFont.menuFontColor[MENU_ACTIVE]);
    }
    cairo_translate(cr, marginLeft + 7, y + (sc->skinFont.menuFontSize / 2.0) );
    cairo_arc(cr, 0, 0, size , 0., 2*M_PI);
    cairo_fill(cr);
    cairo_destroy(cr);
}

/*
* 显示菜单上面的文字信息,只需要指定窗口,窗口宽度,需要显示文字的上边界,字体,显示的字符串和是否选择(选择后反色)
* 其他都固定,如背景和文字反色不反色的颜色,反色框和字的位置等
*/
void DisplayText(XlibMenu * menu,int shellindex,int line_y)
{
    FcitxSkin *sc = &menu->owner->skin;
    int marginLeft = sc->skinMenu.marginLeft;
    int marginRight = sc->skinMenu.marginRight;
    cairo_t *  cr;
    cr=cairo_create(menu->menu_cs);

    SetFontContext(cr, menu->owner->menuFont, sc->skinFont.menuFontSize);

    if (GetMenuShell(menu->menushell, shellindex)->isselect ==0)
    {
        fcitx_cairo_set_color(cr, &sc->skinFont.menuFontColor[MENU_INACTIVE]);

        OutputStringWithContext(cr, GetMenuShell(menu->menushell, shellindex)->tipstr , 15 + marginLeft ,line_y);
    }
    else
    {
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
        fcitx_cairo_set_color(cr, &sc->skinMenu.activeColor);
        cairo_rectangle (cr, marginLeft ,line_y, menu->width - marginRight - marginLeft,sc->skinFont.menuFontSize+4);
        cairo_fill (cr);

        fcitx_cairo_set_color(cr, &sc->skinFont.menuFontColor[MENU_ACTIVE]);
        OutputStringWithContext(cr, GetMenuShell(menu->menushell, shellindex)->tipstr , 15 + marginLeft ,line_y);
    }
    ResetFontContext();
    cairo_destroy(cr);
}

void DrawArrow(XlibMenu *menu, int line_y)
{
    FcitxSkin *sc = &menu->owner->skin;
    int marginRight = sc->skinMenu.marginRight;
    cairo_t* cr=cairo_create(menu->menu_cs);
    double size = sc->skinFont.menuFontSize * 0.4;
    double offset = (sc->skinFont.menuFontSize - size) / 2;
    cairo_move_to(cr,menu->width - marginRight - 1 - size, line_y + offset);
    cairo_line_to(cr,menu->width - marginRight - 1 - size, line_y+size * 2 + offset);
    cairo_line_to(cr,menu->width - marginRight - 1, line_y + size + offset );
    cairo_line_to(cr,menu->width - marginRight - 1 - size ,line_y + offset);
    cairo_fill (cr);
    cairo_destroy(cr);
}

/**
*返回鼠标指向的菜单在menu中是第多少项
*/
int SelectShellIndex(XlibMenu * menu, int x, int y, int* offseth)
{
    FcitxSkin *sc = &menu->owner->skin;
    int i;
    int winheight=sc->skinMenu.marginTop;
    int fontheight;
    int marginLeft = sc->skinMenu.marginLeft;

    if (x < marginLeft)
        return -1;

    fontheight= sc->skinFont.menuFontSize;
    for (i=0;i<utarray_len(&menu->menushell->shell);i++)
    {
        if (GetMenuShell(menu->menushell, i)->type == MENUTYPE_SIMPLE || GetMenuShell(menu->menushell, i)->type == MENUTYPE_SUBMENU)
        {
            if (y>winheight+1 && y<winheight+6+fontheight-1)
            {
                if (offseth)
                    *offseth = winheight;
                return i;
            }
            winheight=winheight+6+fontheight;
        }
        else if (GetMenuShell(menu->menushell, i)->type == MENUTYPE_DIVLINE)
            winheight+=5;
    }
    return -1;
}

boolean ReverseColor(XlibMenu * menu,int shellIndex)
{
    boolean flag = False;
    int i;

    int last = -1;

    for (i=0;i<utarray_len(&menu->menushell->shell);i++)
    {
        if (GetMenuShell(menu->menushell, i)->isselect)
            last = i;

        GetMenuShell(menu->menushell, i)->isselect=0;
    }
    if (shellIndex == last)
        flag = True;
    if (shellIndex >=0 && shellIndex < utarray_len(&menu->menushell->shell))
        GetMenuShell(menu->menushell, shellIndex)->isselect = 1;
    return flag;
}

void ClearSelectFlag(XlibMenu * menu)
{
    int i;
    for (i=0;i< utarray_len(&menu->menushell->shell);i++)
    {
        GetMenuShell(menu->menushell, i)->isselect=0;
    }
}

void ReloadXlibMenu(void* arg, boolean enabled)
{
    XlibMenu* menu = (XlibMenu*) arg;
    boolean visable = WindowIsVisable(menu->owner->dpy, menu->menuWindow);
    cairo_surface_destroy(menu->menu_cs);
    XFreePixmap(menu->owner->dpy, menu->pixmap);
    XDestroyWindow(menu->owner->dpy, menu->menuWindow);

    menu->menu_cs = NULL;
    menu->pixmap = None;
    menu->menuWindow = None;
    
    InitXlibMenu(menu);
    if (visable)
        XMapWindow(menu->owner->dpy, menu->menuWindow);
}

void MoveSubMenu(XlibMenu *sub, XlibMenu *parent, int offseth)
{
    int dwidth, dheight;
    FcitxSkin *sc = &parent->owner->skin;
    GetScreenSize(parent->owner, &dwidth, &dheight);
    UpdateMenuShell(sub->menushell);
    GetMenuSize(sub);
    sub->iPosX=parent->iPosX + parent->width - sc->skinMenu.marginRight - 4;
    sub->iPosY=parent->iPosY + offseth - sc->skinMenu.marginTop;

    if ( sub->iPosX + sub->width > dwidth)
        sub->iPosX=parent->iPosX - sub->width + sc->skinMenu.marginLeft + 4;

    if ( sub->iPosY + sub->height > dheight)
        sub->iPosY = dheight - sub->height;
    
    XMoveWindow(parent->owner->dpy, sub->menuWindow, sub->iPosX, sub->iPosY);
}

