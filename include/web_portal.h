#ifndef __WEB_PORTAL_H__
#define __WEB_PORTAL_H__

#include <WiFiManager.h>

// 在 WiFiManager 自己的路由注册之前，把自定义页面和 JSON 接口挂到同一个 WebServer 上。
// 必须在 wm.autoConnect() / wm.startConfigPortal() 之前调用一次。
void web_portal_install(WiFiManager* wm);

#endif
