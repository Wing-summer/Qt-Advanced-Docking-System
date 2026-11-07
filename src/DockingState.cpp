// SPDX-License-Identifier: LGPL-2.1-or-later
#include "DockingState.h"

#include <QBuffer>
#include <QDataStream>
#include <QSet>

#include <limits>

namespace ads
{
namespace state
{
namespace
{
constexpr quint32 Magic = 0x42534441;  // ADSB
constexpr quint16 FormatVersion = 1;
constexpr int MaxDepth = 256;

void configure(QDataStream& stream)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    stream.setVersion(QDataStream::Qt_6_0);
#else
    // Qt_6_0 = 20. This codec only streams fixed-width primitive integers;
    // byte/string lengths are explicit, so older Qt uses identical wire data.
    stream.setVersion(20);
#endif
    stream.setByteOrder(QDataStream::LittleEndian);
}

// Explicit lengths let the reader validate before allocating (including strings).
void writeBytes(QDataStream& stream, const QByteArray& bytes)
{
    stream << quint32(bytes.size());
    stream.writeRawData(bytes.constData(), int(bytes.size()));
}
void writeString(QDataStream& stream, const QString& text)
{
    writeBytes(stream, text.toUtf8());
}
void writeWidget(QDataStream& stream, const Widget& widget)
{
    writeString(stream, widget.name);
    stream << quint8(widget.closed);
}
void writeNode(QDataStream& stream, const Node& node)
{
    stream << quint8(node.splitter);
    if (node.splitter)
    {
        stream << quint8(node.orientation) << quint32(node.children.size());
        for (int i = 0; i < node.children.size(); ++i)
        {
            stream << qint32(node.sizes[i]);
            writeNode(stream, node.children[i]);
        }
    }
    else
    {
        writeString(stream, node.current);
        stream << quint32(node.allowedAreas) << quint32(node.flags)
               << quint32(node.widgets.size());
        for (const auto& widget : node.widgets)
        {
            writeWidget(stream, widget);
        }
    }
}

class Reader
{
public:
    QBuffer buffer;
    QDataStream stream;
    QSet<QString> names;
    explicit Reader(const QByteArray& bytes) : stream(&buffer)
    {
        buffer.setData(bytes);
        buffer.open(QIODevice::ReadOnly);
        configure(stream);
    }
    bool good() const { return stream.status() == QDataStream::Ok; }
    bool count(quint32& value, int minimumBytes)
    {
        stream >> value;
        return good() && value <= quint64(buffer.bytesAvailable() / minimumBytes)
               && value <= quint32(std::numeric_limits<int>::max());
    }
    bool bytes(QByteArray& value)
    {
        quint32 size;
        if (!count(size, 1))
        {
            return false;
        }
        value.resize(int(size));
        return stream.readRawData(value.data(), int(size)) == int(size);
    }
    bool string(QString& value)
    {
        QByteArray utf8;
        if (!bytes(utf8))
        {
            return false;
        }
        value = QString::fromUtf8(utf8);
        return value.toUtf8() == utf8;
    }
    bool boolean(bool& value)
    {
        quint8 byte;
        stream >> byte;
        value = byte != 0;
        return good() && byte <= 1;
    }
    bool widget(Widget& value)
    {
        if (!string(value.name) || value.name.isEmpty()
            || names.contains(value.name) || !boolean(value.closed))
        {
            return false;
        }
        names.insert(value.name);
        return true;
    }
    bool node(Node& value, int depth)
    {
        if (depth > MaxDepth || !boolean(value.splitter))
        {
            return false;
        }
        quint32 size;
        if (value.splitter)
        {
            quint8 orientation;
            stream >> orientation;
            if ((orientation != Qt::Horizontal && orientation != Qt::Vertical)
                || !count(size, 5))
            {
                return false;
            }
            value.orientation = Qt::Orientation(orientation);
            for (quint32 i = 0; i < size; ++i)
            {
                qint32 pixels;
                stream >> pixels;
                Node child;
                if (!good() || pixels < 0 || !node(child, depth + 1))
                {
                    return false;
                }
                value.sizes.append(pixels);
                value.children.append(std::move(child));
            }
        }
        else
        {
            quint32 allowed, flags;
            if (!string(value.current))
            {
                return false;
            }
            stream >> allowed >> flags;
            if (!good() || (allowed & ~quint32(AllDockAreas | AutoHideDockAreas))
                || (flags & ~quint32(1)) || !count(size, 6))
            {
                return false;
            }
            value.allowedAreas = int(allowed);
            value.flags = int(flags);
            bool hasCurrent = value.current.isEmpty();
            for (quint32 i = 0; i < size; ++i)
            {
                Widget entry;
                if (!widget(entry))
                {
                    return false;
                }
                hasCurrent |= entry.name == value.current;
                value.widgets.append(std::move(entry));
            }
            if (!hasCurrent)
            {
                return false;
            }
        }
        return good();
    }
};
}  // namespace

QByteArray encode(const Layout& layout, int userVersion)
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    configure(stream);
    stream << Magic << FormatVersion << qint32(userVersion);
    writeString(stream, layout.centralWidget);
    writeString(stream, layout.focusedWidget);
    stream << quint32(layout.containers.size());
    for (const auto& container : layout.containers)
    {
        stream << quint8(container.floating);
        writeBytes(stream, container.geometry);
        writeNode(stream, container.root);
        stream << quint32(container.sideBars.size());
        for (const auto& entry : container.sideBars)
        {
            stream << quint8(entry.location) << qint32(entry.size);
            writeWidget(stream, entry.widget);
        }
    }
    return stream.status() == QDataStream::Ok ? data : QByteArray();
}

bool decode(const QByteArray& data, int userVersion, Layout& layout)
{
    Reader reader(data);
    quint32 magic;
    quint16 format;
    qint32 version;
    reader.stream >> magic >> format >> version;
    if (!reader.good() || magic != Magic || format != FormatVersion
        || version != userVersion)
    {
        return false;
    }
    Layout result;
    quint32 count;
    if (!reader.string(result.centralWidget)
        || !reader.string(result.focusedWidget) || !reader.count(count, 12)
        || count == 0)
    {
        return false;
    }
    for (quint32 i = 0; i < count; ++i)
    {
        Container container;
        if (!reader.boolean(container.floating) || container.floating != (i != 0)
            || !reader.bytes(container.geometry)
            || (container.floating
                && (container.geometry.size() != 66
                    || !container.geometry.startsWith(
                        QByteArray::fromHex("01d9d0cb00030000"))))
            || (!container.floating && !container.geometry.isEmpty())
            || !reader.node(container.root, 0) || !container.root.splitter)
        {
            return false;
        }
        quint32 sideCount;
        if (!reader.count(sideCount, 11))
        {
            return false;
        }
        for (quint32 j = 0; j < sideCount; ++j)
        {
            quint8 location;
            qint32 pixels;
            reader.stream >> location >> pixels;
            SideBarWidget entry;
            if (!reader.good() || location > 3 || pixels < 0
                || !reader.widget(entry.widget))
            {
                return false;
            }
            entry.location = location;
            entry.size = pixels;
            container.sideBars.append(std::move(entry));
        }
        result.containers.append(std::move(container));
    }
    if (!reader.good() || !reader.buffer.atEnd()
        || (!result.centralWidget.isEmpty()
            && !reader.names.contains(result.centralWidget)))
    {
        return false;
    }
    layout = std::move(result);
    return true;
}
}  // namespace state
}  // namespace ads
