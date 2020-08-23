/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2019-2020 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sérgio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  Contact KDAB at <info@kdab.com> for commercial licensing options.
*/

#include "SegmentedIndicators_p.h"
#include "DropArea_p.h"
#include "Config.h"

#include <QPainter>
#include <QPainterPath>

#define SEGMENT_GIRTH 50
#define SEGMENT_PEN_WIDTH 4

using namespace KDDockWidgets;


SegmentedIndicators::SegmentedIndicators(DropArea *dropArea)
    : DropIndicatorOverlayInterface(dropArea)
{
    // If the app didn't choose opacity then we choose a suitable default value.
    // ClassicIndicators works fine with an opaque dragged window because the indicators have higher Z,
    // However for SegmentedIndicators the indicators are in the main window, so lower Z. Make the
    // dragged window translucent a bit, so we can see the indicators
    const bool userChoseOpacity = !qIsNaN(Config::self().draggedWindowOpacity());
    if (!userChoseOpacity)
        Config::self().setDraggedWindowOpacity(0.7);
}

SegmentedIndicators::~SegmentedIndicators()
{
}

void SegmentedIndicators::hover_impl(QPoint pt)
{
    m_hoveredPt = mapFromGlobal(pt);
    updateSegments();
    setCurrentDropLocation(dropLocationForPos(m_hoveredPt));
}

DropIndicatorOverlayInterface::DropLocation SegmentedIndicators::dropLocationForPos(QPoint pos) const
{
    for (auto it = m_segments.cbegin(), end = m_segments.cend(); it != end; ++it) {
        if (it.value().containsPoint(pos, Qt::OddEvenFill)) {
            return it.key();
        }
    }

    return DropLocation_None;
}

void SegmentedIndicators::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    drawSegments(&p);
}

QVector<QPolygon> SegmentedIndicators::segmentsForRect(QRect r, QPolygon &center, bool useOffset) const
{
    const int penWidth = SEGMENT_PEN_WIDTH;
    const int halfPenWidth = penWidth / 2;

    const int l = SEGMENT_GIRTH;
    const int top = (r.y() == 0 && useOffset) ? l : r.y();
    const int left = (r.x() == 0 && useOffset) ? l : r.x();
    const int right = (rect().right() == r.right() && useOffset) ? r.right() - l : r.right();
    const int bottom = (rect().bottom() == r.bottom() && useOffset) ? r.bottom() - l : r.bottom();
    const QPoint topLeft = { left + halfPenWidth, top + halfPenWidth };
    const QPoint topRight = { right, top + halfPenWidth };
    const QPoint bottomLeft = { left + halfPenWidth, bottom };
    const QPoint bottomRight = { right, bottom };

    const QVector<QPoint> leftPoints = { topLeft, bottomLeft,
                                         QPoint(left, bottom) + QPoint(l, -l),
                                         topLeft + QPoint(l, l), topLeft
                                       };

    const QVector<QPoint> rightPoints = { topRight, bottomRight,
                                          bottomRight + QPoint(-l, -l),
                                          topRight + QPoint(-l, l)
                                        };

    const QVector<QPoint> topPoints = { topLeft, topRight,
                                        topRight + QPoint(-l, l),
                                        topLeft + QPoint(l, l)
                                      };

    const QVector<QPoint> bottomPoints = { bottomLeft, bottomRight,
                                           bottomRight + QPoint(-l, -l),
                                           bottomLeft + QPoint(l, -l)
                                         };

    {

        QPolygon bounds = QVector<QPoint> { topLeft + QPoint(l, l),
                                            topRight + QPoint(-l, l),
                                            bottomRight + QPoint(-l, -l),
                                            bottomLeft + QPoint(l, -l)
                                          };
        const int maxWidth = bounds.boundingRect().width();
        const QPoint centerPos = bounds.boundingRect().center();

        // Build the center
        const int indicatorWidth = qMin(300, maxWidth - 100);
        const int indicatorHeight = qMin(160, int(indicatorWidth * 0.60));
        const int tabWidth = int(indicatorWidth * 0.267);
        const int tabHeight = int(indicatorHeight * 0.187);
        const int centerRectLeft = centerPos.x() - indicatorWidth / 2;
        const int centerRectRight = centerPos.x() + indicatorWidth / 2;
        const int centerRectBottom = centerPos.y() + indicatorHeight / 2;
        const int centerRectTop = centerPos.y() - indicatorHeight / 2;


        center = QVector<QPoint> { { centerRectLeft, centerRectTop },
                                   { centerRectLeft + tabWidth, centerRectTop },
                                   { centerRectLeft + tabWidth, centerRectTop + tabHeight },
                                   { centerRectRight, centerRectTop + tabHeight },
                                   { centerRectRight, centerRectBottom },
                                   { centerRectLeft, centerRectBottom },
                                 };
    }

    return { leftPoints, topPoints, rightPoints, bottomPoints };
}

void SegmentedIndicators::updateSegments()
{
    m_segments.clear();

    const bool hasMultipleFrames = m_dropArea->count() > 1;
    const bool needsInnerIndicators = hoveredFrameRect().isValid();
    const bool needsOutterIndicators = hasMultipleFrames || !needsInnerIndicators;
    QPolygon center;

    if (needsInnerIndicators) {
        const bool useOffset = needsOutterIndicators;
        auto segments = segmentsForRect(hoveredFrameRect(), /*by-ref*/center, useOffset);
        for (int i = 0; i < 4; ++i)
            m_segments.insert(DropLocation(DropLocation_Left + i), segments[i]);

        m_segments.insert(DropLocation_Center, center);
    }

    if (needsOutterIndicators) {
        auto segments = segmentsForRect(rect(), /*unused*/center);
        for (int i = 0; i < 4; ++i)
            m_segments.insert(DropLocation(DropLocation_OutterLeft + i), segments[i]);
    }

    update();
}

void SegmentedIndicators::drawSegments(QPainter *p)
{
    for (int i = DropLocation_First; i <= DropLocation_Last; ++i)
        drawSegment(p, m_segments.value(DropLocation(i)));
}

void SegmentedIndicators::drawSegment(QPainter *p, const QPolygon &segment)
{
    if (segment.isEmpty())
        return;

    QPen pen(Qt::black);
    pen.setWidth(SEGMENT_PEN_WIDTH);
    p->setPen(pen);
    QColor brush(0xbb, 0xd5, 0xee, 200);

    if (segment.containsPoint(m_hoveredPt, Qt::OddEvenFill))
        brush = QColor(0x3574c5);

    p->setBrush(brush);
    p->drawPolygon(segment);
}
