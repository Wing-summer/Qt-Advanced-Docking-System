// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef ADS_DOCKING_STATE_H
#define ADS_DOCKING_STATE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <Qt>

#include "ads_globals.h"

namespace ads
{
namespace state
{
// Value-only snapshot. No QWidget ownership or live pointers enter the codec.
struct Widget
{
    QString name;
    bool closed = false;
};
struct Node
{
    bool splitter = true;
    Qt::Orientation orientation = Qt::Horizontal;
    QList<Node> children;
    QList<int> sizes;
    QList<Widget> widgets;
    QString current;
    int allowedAreas = 0x1f;
    int flags = 0;
};
struct SideBarWidget
{
    Widget widget;
    int location = 0;
    int size = 0;
};
struct Container
{
    bool floating = false;
    QByteArray geometry;
    Node root;
    QList<SideBarWidget> sideBars;
};
struct Layout
{
    QString centralWidget;
    QString focusedWidget;
    QList<Container> containers;
};
ADS_EXPORT QByteArray encode(const Layout& layout, int userVersion);
ADS_EXPORT bool decode(const QByteArray& data, int userVersion, Layout& layout);
}  // namespace state
}  // namespace ads
#endif
