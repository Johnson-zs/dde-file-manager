#ifndef DFMEXTWINDOWPROXY_H
#define DFMEXTWINDOWPROXY_H

#include <dfm-extension/dfm-extension-global.h>
#include <string>
#include <vector>

BEGEN_DFMEXT_NAMESPACE

// TODO: impl me

class DFMExtWindow;
class DFMExtWindowProxyPrivate;
class DFMExtWindowProxy
{
    friend class DFMExtWindowProxyPrivate;

public:
    ~DFMExtWindowProxy();

    DFMExtWindow *createWindow(const std::string &urlString);
    void showWindow(std::uint64_t winId);
    std::vector<std::uint64_t> windowIdList();

protected:
    explicit DFMExtWindowProxy(DFMExtWindowProxyPrivate *d_ptr);
    DFMExtWindowProxyPrivate *d;
};

END_DFMEXT_NAMESPACE

#endif   // DFMEXTWINDOWPROXY_H
