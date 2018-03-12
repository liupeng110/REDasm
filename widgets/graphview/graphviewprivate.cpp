#include "graphviewprivate.h"
#include <QGraphicsDropShadowEffect>
#include <QStack>
#include <cmath>

#define DROP_SHADOW_SIZE(x) x, x, x, x
#define DROP_SHADOW_VALUE 8
#define DROP_SHADOW_ARG   DROP_SHADOW_SIZE(DROP_SHADOW_VALUE)
#define ZOOM_FACTOR_STEP  0.050
#define ITEM_PADDING      25
#define BORDER_PADDING    3
#define ANGLE_SIZE        (ITEM_PADDING / 2)
#define ARROW_SIZE        6.0

GraphViewPrivate::GraphViewPrivate(QWidget *parent) : QWidget(parent), _overviewmode(false), _zoomfactor(1.0), _graph(NULL), _lgraph(NULL)
{
    QPalette p = this->palette();
    p.setColor(QPalette::Background, QColor("azure"));
    this->setAutoFillBackground(true);
    this->setPalette(p);

    this->_graphsize = QSize(0, 0);
}

const QSize &GraphViewPrivate::graphSize() const
{
    return this->_graphsize;
}

void GraphViewPrivate::addItem(GraphItem *item)
{
    item->setParent(this); // Take ownership

    this->_items << item;
    this->_itembyid[item->vertex()->id] = item;
}

void GraphViewPrivate::removeAll()
{
    this->_graphsize = QSize(0, 0);

    if(!this->_items.isEmpty())
    {
        qDeleteAll(this->_items);
        this->_items.clear();
    }

    this->update();
}

u64 GraphViewPrivate::itemPadding() const
{
    return ITEM_PADDING;
}

bool GraphViewPrivate::overviewMode() const
{
    return this->_overviewmode;
}

void GraphViewPrivate::setOverviewMode(bool b)
{
    this->_overviewmode = b;
}

void GraphViewPrivate::setGraph(REDasm::Graphing::Graph *graph)
{
    this->_graph = graph;
    this->_lgraph.setGraph(graph);
}

void GraphViewPrivate::drawArrow(QPainter *painter, GraphItem *fromitem, GraphItem *toitem)
{
    QRect fromrect = fromitem->rect(), torect = toitem->rect();
    QPoint fromcenter = fromrect.center(), tocenter = torect.center();
    double layerheight = this->getLayerHeight(fromitem);

    QPoint points[5];
    points[0] = QPoint(fromcenter.x(), fromrect.bottom() + BORDER_PADDING);
    points[1] = QPoint(fromcenter.x(), fromrect.top() + layerheight);
    points[2] = QPoint(fromcenter.x(), fromrect.top() + layerheight + ANGLE_SIZE);
    points[3] = QPoint(tocenter.x(), fromrect.top() + layerheight + ANGLE_SIZE);
    points[4] = QPoint(tocenter.x(), tocenter.y() - BORDER_PADDING);
    painter->drawPolyline(points, 5);

    if(toitem->vertex()->isFake())
    {
        painter->drawLine(points[4], QPoint(tocenter.x(), tocenter.y() + BORDER_PADDING));
        return;
    }

    QPolygonF arrowhead;
    arrowhead << QPoint(points[4].x() - ARROW_SIZE, torect.top() - ARROW_SIZE)
              << QPoint(points[4].x() + ARROW_SIZE, torect.top() - ARROW_SIZE)
              << QPoint(points[4].x(), torect.top());

    painter->drawPolygon(arrowhead);
}

void GraphViewPrivate::drawEdges(QPainter *painter, GraphItem* item)
{
    const REDasm::Graphing::Vertex* v1 = item->vertex();
    painter->save();

    for(REDasm::Graphing::vertex_id_t edge : v1->edges)
    {
        if(!this->_itembyid.contains(edge))
            continue;

        REDasm::Graphing::Vertex *rv1 = this->_graph->getRealParentVertex(v1->id), *rv2 = this->_graph->getRealVertex(edge);
        QColor c(QString::fromStdString(rv1->edgeColor(rv2)));

        painter->setPen(QPen(c, 2));
        painter->setBrush(c);
        this->drawArrow(painter, item, this->_itembyid[edge]);
    }

    painter->restore();
}

void GraphViewPrivate::setGraphSize(const QSize &size)
{
    this->_graphsize = size;

    emit graphChanged();
    this->update();
}

double GraphViewPrivate::getLayerHeight(GraphItem *item)
{
    if(!this->_layerheight.contains(item->layer()))
    {
        double maxheight = 0;

        for(REDasm::Graphing::Vertex* v : this->_lgraph[item->layer()])
        {
            GraphItem* litem = this->_itembyid[v->id];
            maxheight = std::max(maxheight, static_cast<double>(litem->size().height()));
        }

        this->_layerheight[item->layer()] = maxheight;
    }

    return this->_layerheight[item->layer()];
}

void GraphViewPrivate::paintEvent(QPaintEvent*)
{
    if(!this->_graph)
        return;

    this->_layerheight.clear();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.eraseRect(this->rect());
    painter.scale(this->_zoomfactor, this->_zoomfactor);

    foreach(GraphItem* item, this->_items)
    {
        if(!item->vertex()->isFake())
        {
            painter.fillRect(item->rect().adjusted(DROP_SHADOW_ARG), Qt::lightGray);
            painter.fillRect(item->rect(), QColor("white"));

            painter.save();
            painter.setClipRect(item->rect().adjusted(-1, -1, 1, 1));
            item->paint(&painter);

            painter.setPen(QPen(item->borderColor(), 2));
            painter.drawRect(item->rect());
            painter.restore();
        }

        this->drawEdges(&painter, item);
    }
}

void GraphViewPrivate::wheelEvent(QWheelEvent *event)
{
    QWidget::wheelEvent(event);

    if(event->modifiers() & Qt::ControlModifier)
    {
        if(event->delta() > 0)
            this->_zoomfactor += ZOOM_FACTOR_STEP;
        else if(event->delta() < 0)
            this->_zoomfactor -= ZOOM_FACTOR_STEP;
        else
            return;

        if(this->_zoomfactor < 0.005)
            this->_zoomfactor = 0.005;
        else if(this->_zoomfactor > 2.005)
            this->_zoomfactor = 2.005;

        this->update();
    }
}
