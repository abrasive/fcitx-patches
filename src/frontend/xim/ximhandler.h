/***************************************************************************
 *   Copyright (C) 2010~2010 by CSSlayer                                   *
 *   wengxt@gmail.com                                                      *
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

#ifndef _FCITX_XIMHANDLER_H_
#define _FCITX_XIMHANDLER_H_

#include <X11/Xlib.h>
#include <Xi18n.h>

struct _FcitxXimIC;

Bool XIMOpenHandler(FcitxXimFrontend* xim, IMOpenStruct * call_data);
Bool XIMGetICValuesHandler(FcitxXimFrontend* xim, IMChangeICStruct * call_data);
Bool XIMSetICValuesHandler(FcitxXimFrontend* xim, IMChangeICStruct * call_data);
Bool XIMSetFocusHandler(FcitxXimFrontend* xim, IMChangeFocusStruct * call_data);
Bool XIMUnsetFocusHandler(FcitxXimFrontend* xim, IMChangeICStruct * call_data);
Bool XIMCloseHandler(FcitxXimFrontend* xim, IMOpenStruct * call_data);
Bool XIMCreateICHandler(FcitxXimFrontend* xim, IMChangeICStruct * call_data);
Bool XIMDestroyICHandler(FcitxXimFrontend* xim, IMChangeICStruct * call_data);
Bool XIMTriggerNotifyHandler(FcitxXimFrontend* xim, IMTriggerNotifyStruct * call_data);
void XIMProcessKey(FcitxXimFrontend* xim, IMForwardEventStruct * call_data);
void XimForwardKeyInternal(FcitxXimFrontend *xim, struct _FcitxXimIC* ic, XEvent* xEvent );

#endif
