// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: CC0-1.0
#include <DApplication>

DWIDGET_USE_NAMESPACE

#ifdef DFM_ORGANIZATION_NAME
#    define ORGANIZATION_NAME DFM_ORGANIZATION_NAME
#else
#    define ORGANIZATION_NAME "deepin"
#endif

int main(int argc, char *argv[])
{
    DApplication a(argc, argv);
    a.setApplicationVersion("1.0.0");
    a.setOrganizationName(ORGANIZATION_NAME);
    a.setApplicationName("dde-desktop-widgets-viewer");

    // TODO(widgets): translation

    return a.exec();
}
