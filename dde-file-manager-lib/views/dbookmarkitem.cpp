/*
 * Copyright (C) 2016 ~ 2018 Deepin Technology Co., Ltd.
 *               2016 ~ 2018 dragondjf
 *
 * Author:     dragondjf<dingjiangfeng@deepin.com>
 *
 * Maintainer: dragondjf<dingjiangfeng@deepin.com>
 *             zccrs<zhangjide@deepin.com>
 *             Tangtong<tangtong@deepin.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dbookmarkitem.h"
#include "dbookmarkitemgroup.h"

#include "dfilemenu.h"
#include "dfilemenumanager.h"
#include "windowmanager.h"
#include "ddragwidget.h"
#include "ddialog.h"
#include "dbookmarkmountedindicatoritem.h"

#include <QDebug>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QSvgRenderer>
#include <QVariantAnimation>


#include "models/bookmark.h"

#include "app/define.h"
#include "dfmevent.h"
#include "app/filesignalmanager.h"
#include "models/desktopfileinfo.h"

#include "controllers/bookmarkmanager.h"
#include "controllers/appcontroller.h"
#include "deviceinfo/udisklistener.h"
#include "dfileservices.h"

#include "dialogs/dialogmanager.h"
#include "singleton.h"

#include "plugins/pluginmanager.h"
#include "view/viewinterface.h"
#include "shutil/fileutils.h"

#include "gvfs/networkmanager.h"
#include "dfmglobal.h"
#include "dbookmarkscene.h"
#include "dutil.h"
#include "utils/utils.h"

#include "tag/tagutil.h"
#include "tag/tagmanager.h"
#include "interfaces/dfmplaformmanager.h"



DWIDGET_USE_NAMESPACE


int DBookmarkItem::DEFAULT_ICON_SIZE = 16;

DBookmarkItem::DBookmarkItem(const QString &key)
    : m_key(key)
{
    ///###: install a event filter.
    this->installEventFilter(this);

    init();
    m_isDefault = true;
}

DBookmarkItem::DBookmarkItem(UDiskDeviceInfoPointer deviceInfo)
{
    init();
    setDeviceInfo(deviceInfo);
}

DBookmarkItem::DBookmarkItem(BookMark *bookmark)
    : DBookmarkItem("BookMarks")
{
    m_isDefault = false;
    m_url = bookmark->getUrl();
    m_textContent = bookmark->getName();
}

void DBookmarkItem::setDeviceInfo(UDiskDeviceInfoPointer deviceInfo)
{
    setHighlightDiskBackgroundEnable(true);
    m_isDisk = true;
    m_checkable = true;
    m_url = deviceInfo->getMountPointUrl();
    m_isDefault = true;
    m_deviceID = deviceInfo->getDiskInfo().id();
    m_textContent = deviceInfo->fileDisplayName();
    m_isMounted = deviceInfo->getDiskInfo().can_unmount();
    m_deviceInfo = deviceInfo;

    if (!m_mountBookmarkItem) {
        m_mountBookmarkItem = makeMountBookmark(this);
    }
    updateMountIndicator();
}

void DBookmarkItem::init()
{
    setAcceptHoverEvents(true);
//    setReleaseBackgroundColor(QColor(238,232,205,0));
//    setPressBackgroundColor(QColor("#D5EDFE"));
//    setHoverBackgroundColor(QColor(0, 0, 0, 12));
//    setHighlightDiskBackgroundColor(QColor("#E9E9E9"));
    setPressBackgroundEnable(true);
    setReleaseBackgroundEnable(true);
    setHoverBackgroundEnable(true);
    setHighlightDiskBackgroundEnable(false);
//    setTextColor(Qt::black);
//    setTextHoverColor(Qt::black);
//    setTextPressColor(QColor("#2ca7f8"));
//    setTextCheckedColor(QColor("#2ca7f8"));
    setAcceptDrops(true);
    m_checkable = true;
}

QPixmap DBookmarkItem::getPixmap(const QString &key, ThemeConfig::State state) const
{
    QPixmap p;

    if (!m_key.isEmpty()) {
        p = ThemeConfig::instace()->pixmap("BookmarkItem." + m_key, key, state);
    }

    if (p.isNull()) {
        p = ThemeConfig::instace()->pixmap("BookmarkItem", key, state);
    }

    return p;
}

bool DBookmarkItem::getIsCustomBookmark() const
{
    return m_isCustomBookmark;
}

void DBookmarkItem::setIsCustomBookmark(bool isCustomBookmark)
{
    m_isCustomBookmark = isCustomBookmark;
}

void DBookmarkItem::changeIconThroughColor(const QColor &color)noexcept
{
    if (color.isValid()) {
        QString newColor{ TagManager::instance()->getColorNameByColor(color) };

        this->m_key = newColor;
        this->update();
    }
}

bool DBookmarkItem::getMountBookmark() const
{
    return m_mountBookmark;
}

void DBookmarkItem::setMountBookmark(bool mountBookmark)
{
    m_mountBookmark = mountBookmark;
}
bool DBookmarkItem::isMountedIndicator() const
{
    return m_isMountedIndicator;
}

void DBookmarkItem::setIsMountedIndicator(bool isMountedIndicator)
{
    m_isMountedIndicator = isMountedIndicator;
}


void DBookmarkItem::editFinished()
{
    if (!m_lineEdit) {
        return;
    }

    QSignalBlocker block_lineEdit(m_lineEdit);
    Q_UNUSED(block_lineEdit)

    DFMUrlBaseEvent event(this, m_url);
    event.setWindowId(windowId());
    DUrl oldTag;
    oldTag.setScheme(TAG_SCHEME);
    oldTag.setPath(m_textContent);

    if (m_url.isTaggedFile()) {
        DUrl newTag;
        newTag.setScheme(TAG_SCHEME);
        newTag.setPath(m_lineEdit->text());

        if (oldTag != newTag)
            DFileService::instance()->renameFile(nullptr, oldTag, newTag);
    } else {
        if (!m_lineEdit->text().isEmpty() && m_lineEdit->text() != m_textContent) {
            bookmarkManager->renameBookmark(getBookmarkModel(), m_lineEdit->text());
            emit fileSignalManager->bookmarkRenamed(m_lineEdit->text(), event);
            m_textContent = m_lineEdit->text();
        }
    }

    m_widget->deleteLater();
    m_lineEdit = Q_NULLPTR;

//    emit fileSignalManager->requestFoucsOnFileView(windowId());
}

void DBookmarkItem::checkMountedItem(const DFMEvent &event)
{
    if (event.windowId() != windowId()) {
        return;
    }

    if (m_isDisk && m_deviceInfo) {
        qDebug() << event;

        if (m_group) {
            m_group->deselectAll();
            setChecked(true);
        }
        m_pressed = false;
        update();

        DFMEvent e(this);
        e.setWindowId(windowId());
        e.setData(m_url);
        qDebug() << m_isDisk << m_deviceInfo << m_url;
        m_group->url(e);
    }
}

void DBookmarkItem::playAnimation()
{
    setTransformOriginPoint(26, size().height() / 2);
    QVariantAnimation *scaleUpAni = new QVariantAnimation(this);
    scaleUpAni->setStartValue(qreal(1.0));
    scaleUpAni->setEndValue(qreal(1.2));
    scaleUpAni->setDuration(80);

    QVariantAnimation *scaleDownAni = new QVariantAnimation(this);
    scaleDownAni->setStartValue(qreal(1.2));
    scaleDownAni->setEndValue(qreal(1.0));
    scaleDownAni->setDuration(220);

    connect(scaleUpAni, &QVariantAnimation::valueChanged, [ = ](const QVariant & fac) {
        setScale(fac.toReal());
    });
    connect(scaleDownAni, &QVariantAnimation::valueChanged, [ = ](const QVariant & fac) {
        setScale(fac.toReal());
    });
    connect(scaleUpAni, &QVariantAnimation::finished, [ = ] {
        scaleDownAni->start();
        scaleUpAni->deleteLater();
    });
    connect(scaleDownAni, &QVariantAnimation::finished, [ = ] {
        scaleDownAni->deleteLater();
    });
    scaleUpAni->start();
}

QRectF DBookmarkItem::boundingRect() const
{
    return QRectF(m_x_axis - m_adjust,
                  m_y_axis - m_adjust,
                  m_width + m_adjust,
                  m_height + m_adjust);
}

void DBookmarkItem::setTightMode(bool v)
{
    m_isTightMode = v;
}

bool DBookmarkItem::isTightModel()
{
    return m_isTightMode;
}

void DBookmarkItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    painter->setClipRect(option->exposedRect);
//    double w = m_width;
    QColor textColor;
    double leftPadding = 13;
    double dy = (m_height - DEFAULT_ICON_SIZE) / 2;
    QPixmap press;
    QPixmap checked;
    QPixmap release;
    QPixmap hover;

    QRect iconRect(leftPadding, dy, DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE);

    if (m_isTightMode) {
//        press = m_pressImageBig;
        press = getPixmap("icon.big", ThemeConfig::Checked);
        if (press.isNull()) {
            checked = getPixmap("icon.big", ThemeConfig::Pressed);
        } else {
            checked = press;
        }

//        release = m_releaseImageBig;
        release = getPixmap("icon.big");
//        hover = m_hoverImageBig;
        hover = getPixmap("icon.big", ThemeConfig::Hover);

        dy = m_height / 2 - release.height() / 2;
    } else {
        press = getPixmap("icon", ThemeConfig::Checked);
        if (press.isNull()) {
            checked = getPixmap("icon", ThemeConfig::Pressed);
        } else {
            checked = press;
        }

//        release = m_releaseImage;
        release = getPixmap("icon");
//        hover = m_hoverImage;
        hover = getPixmap("icon", ThemeConfig::Hover);
        dy = m_height / 2 - release.height() / 2;
    }

    painter->setPen(Qt::transparent);

    if (m_hovered && !m_isHighlightDisk) {
        if (m_hoverBackgroundEnabled && !m_isMountedIndicator) {
            painter->setBrush(ThemeConfig::instace()->color("BookmarkItem", "background", ThemeConfig::Hover));
//            painter->setPen(QColor(0,0,0,0));
            painter->drawRect(m_x_axis, m_y_axis, m_width, m_height);
            textColor = ThemeConfig::instace()->color("BookmarkItem", "color", ThemeConfig::Hover);
        }
        painter->drawPixmap(iconRect, hover);
    } else if (m_pressed) {
        if (m_pressBackgroundEnabled && !m_isMountedIndicator) {
//            painter->setPen(m_pressBackgroundColor);
            painter->setBrush(ThemeConfig::instace()->color("BookmarkItem", "background", ThemeConfig::Pressed));
            painter->drawRect(m_x_axis, m_y_axis, m_width, m_height);
            textColor = ThemeConfig::instace()->color("BookmarkItem", "color", ThemeConfig::Pressed);
        }
        painter->drawPixmap(iconRect, press);
    } else if (!m_hovered && (m_checked && m_checkable)) {
        if (m_pressBackgroundEnabled && !m_isMountedIndicator) {
//            painter->setPen(m_pressBackgroundColor);
            painter->setBrush(ThemeConfig::instace()->color("BookmarkItem", "background", ThemeConfig::Checked));
            painter->drawRect(m_x_axis, m_y_axis, m_width, m_height);
            textColor = ThemeConfig::instace()->color("BookmarkItem", "color", ThemeConfig::Checked);
        }
        painter->drawPixmap(iconRect, checked);
        if (!m_isMountedIndicator) {
            painter->setPen(QColor(43, 167, 248, 25));
            painter->drawLine(0, 0, m_width - 3, 0);
            painter->drawLine(0, m_height - 1, m_width - 3, m_height - 1);
            painter->setBrush(QColor("#2ca7f8"));
            painter->drawRect(m_width - 3, 0, 3, m_height);
        }
    } else if (m_isHighlightDisk) {
        if (m_highlightDiskBackgroundEnabled && !m_isMountedIndicator) {
//            painter->setPen(m_highlightDiskBackgroundColor);
            painter->setBrush(ThemeConfig::instace()->color("BookmarkItem", "background", ThemeConfig::Checked));
            painter->drawRect(m_x_axis, m_y_axis, m_width, m_height);
            textColor = ThemeConfig::instace()->color("BookmarkItem", "color", ThemeConfig::Checked);
        }
        painter->drawPixmap(iconRect, checked);

        if (!m_isMountedIndicator) {
            painter->setPen(QColor(0, 0, 0, 25));
            painter->drawLine(0, 0, m_width - 3, 0);
            painter->drawLine(0, m_height - 1, m_width - 3, m_height - 1);
            painter->setBrush(QColor("#BDBDBD"));
            painter->drawRect(m_width - 3, 0, 3, m_height);
        }
    } else {
        if (m_releaseBackgroundEnabled) {
//            painter->setPen(m_releaseBackgroundColor);
//            painter->setBrush(m_releaseBackgroundColor);
            painter->drawRect(m_x_axis, m_y_axis, m_width, m_height);
        }

        painter->drawPixmap(iconRect, release);
        textColor = ThemeConfig::instace()->color("BookmarkItem", "color");;
    }

    double textTopPadding = 4;
    if (m_checkable && m_checked) {
        m_font.setWeight(QFont::Normal + 10);
    } else {
        m_font.setWeight(QFont::Normal);
    }

    if (!m_isTightMode) {
        painter->setPen(textColor);
        painter->setFont(m_font);
        QRect rect(leftPadding + 24, m_y_axis, m_width - 25, m_height);
        QFontMetrics metrics(m_font);
        QString elidedText = metrics.elidedText(m_textContent, Qt::ElideMiddle, m_width - leftPadding - 60);
        painter->drawText(rect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignVCenter, elidedText);
    } else {
        if (!m_isDefault && !m_textContent.isEmpty()) {
            QString text = m_textContent.at(0);
//            painter->setPen(QColor(0x2CA7F8));
            m_font.setPointSize(8);

            painter->setFont(m_font);

            painter->drawText(leftPadding + 6, m_y_axis + m_height * 3 / 4 - textTopPadding, text);
        }
    }


//    if(m_isMounted)
//    {
//        QPixmap pressPic(":/icons/images/icons/unmount_press.svg");
//        QPixmap normalPic(":/icons/images/icons/unmount_normal.svg");
//        QPixmap hoverPic(":/icons/images/icons/unmount_press.svg");
//        if(m_pressed || (m_checked && m_checkable))
//            painter->drawPixmap(w - 20, pressPic.height()/2,
//                                pressPic.width(), pressPic.height(), pressPic);
//        else if(m_hovered)
//            painter->drawPixmap(w - 20, hoverPic.height()/2,
//                                hoverPic.width(), hoverPic.height(), hoverPic);
//        else
//            painter->drawPixmap(w - 20, normalPic.height()/2,
//                                normalPic.width(), normalPic.height(), normalPic);
//    }
}

void DBookmarkItem::setPressedIcon(const QString &iconPath)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Pressed, iconPath);
}

void DBookmarkItem::setPressedIcon(const QIcon &icon)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Pressed, QVariant::fromValue(icon));
}

void DBookmarkItem::setHoverIcon(const QString &iconPath)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Hover, iconPath);
}

void DBookmarkItem::setHoverIcon(const QIcon &icon)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Hover, QVariant::fromValue(icon));
}

void DBookmarkItem::setReleaseIcon(const QString &iconPath)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Normal, iconPath);
}

void DBookmarkItem::setReleaseIcon(const QIcon &icon)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Normal, QVariant::fromValue(icon));
}

void DBookmarkItem::setCheckedIcon(const QString &iconPath)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Checked, iconPath);
}

void DBookmarkItem::setCheckedIcon(const QIcon &icon)
{
    ThemeConfig::instace()->setValue("BookmarkItem" + m_key, "icon", ThemeConfig::Checked, QVariant::fromValue(icon));
}

void DBookmarkItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (objectName() == "BMRootItem") {
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (m_isDraggable) {
            m_xPress = event->pos().x();
            m_yPress = event->pos().y();
        }
        m_pressed = true;
        m_hovered = false;
        update();
    }
}

void DBookmarkItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (!m_isDraggable)
        return;

//    if (m_pressed && m_isDraggable) {
//        m_xOffset = event->pos().x() - m_xPress;
//        m_xPos += m_xOffset;
//        m_xPress = event->pos().x();

//        m_yOffset = event->pos().y() - m_yPress;
//        m_yPos += m_yOffset;
//        m_yPress = event->pos().y();
//        moveBy(m_xPos, m_yPos);
//        update();
//    }

    if (m_pressed && !m_isDefault && scene()->views().first()->window()->windowType() == Qt::Window) {
        if (QLineF(event->screenPos(), event->buttonDownScreenPos(Qt::LeftButton))
                .length() < QApplication::startDragDistance()) {
            return;
        }

        drag = new DDragWidget((QObject *)event->widget());
        QMimeData *mimeData = new QMimeData;
        drag->setPixmap(toPixmap());
        drag->setMimeData(mimeData);
        drag->setHotSpot(QPoint(m_width / 4, 4));
        drag->startDrag();
        m_pressed = false;
        drag = NULL;
        QPoint point = scene()->views().at(0)->mapFromGlobal(QCursor::pos());
        QPointF point1 = scene()->views().at(0)->mapToScene(point);
        emit dragFinished(QCursor::pos(), point1, this);
    }
}

QPixmap DBookmarkItem::toPixmap()
{
    if (!scene()) {
        return QPixmap();
    }

    QSvgRenderer renderer;

    renderer.load(QString(":/icons/images/icons/reordering_line.svg"));

    QImage image;
    QPainter painter;

    if (m_isTightMode) {
        image = QImage(60, renderer.defaultSize().height(), QImage::Format_ARGB32);
    } else {
        image = QImage(200, renderer.defaultSize().height() * 200.0 / renderer.defaultSize().width(), QImage::Format_ARGB32);
    }

    image.fill(Qt::transparent);
    painter.begin(&image);
    renderer.render(&painter, QRect(QPoint(0, 0), image.size()));

    return QPixmap::fromImage(image);
}

bool DBookmarkItem::isMounted()
{
    return m_isMounted;
}

void DBookmarkItem::setMounted(bool v)
{
    m_isMounted = v;
    update();
    updateMountIndicator();
}

void DBookmarkItem::setDeviceLabel(const QString &label)
{
    m_deviceLabel = label;
}

QString DBookmarkItem::getDeviceLabel()
{
    return m_deviceLabel;
}

void DBookmarkItem::setDeviceID(const QString &deviceID)
{
    m_deviceID = deviceID;
}

QString DBookmarkItem::getDeviceID()
{
    return m_deviceID;
}

int DBookmarkItem::windowId()
{
    if (scene() && scene()->views().count() > 0) {
        return WindowManager::getWindowId(scene()->views().at(0));
    }
    return -1;
}

BookMarkPointer DBookmarkItem::getBookmarkModel()
{
    return m_bookmarkModel;
}

void DBookmarkItem::setBookmarkModel(BookMarkPointer bookmark)
{
    m_bookmarkModel = bookmark;
}

DBookmarkMountedIndicatorItem *DBookmarkItem::makeMountBookmark(DBookmarkItem *parentItem)
{
    DBookmarkMountedIndicatorItem *item = new DBookmarkMountedIndicatorItem(parentItem);
    return item;
}

void DBookmarkItem::editMode()
{
    m_lineEdit = new QLineEdit;

    connect(m_lineEdit, &QLineEdit::editingFinished, this, &DBookmarkItem::editFinished);

    m_widget = scene()->addWidget(m_lineEdit);
    m_lineEdit->setGeometry(37 + geometry().x(), geometry().y(), m_width - 37, m_height);
    m_lineEdit->setText(m_textContent);
    m_lineEdit->setSelection(0, m_textContent.length());
    m_lineEdit->setFocus();
    m_lineEdit->installEventFilter(this);
//    m_lineEdit->setStyleSheet("QLineEdit {\
//                              background: #0b8ade;\
//                              color:white;\
//                              selection-background-color: #70bfff;\
//                              padding-left: 6px;\
//                              border-radius: 4px;\
//                          }");
    m_lineEdit->show();

}

void DBookmarkItem::updateMountIndicator()
{
    qDebug() << m_isMounted << m_mountBookmarkItem;
    if (m_isMounted && m_mountBookmarkItem) {
        m_mountBookmarkItem->show();
        if (DFMGlobal::isRootUser()) {
            m_mountBookmarkItem->setEnabled(false);
        }

        /*Disable unmount of native disk in x86 pro*/
        if (DFMGlobal::isDisableUnmount(m_deviceInfo->getDiskInfo())) {
            m_mountBookmarkItem->setEnabled(false);
            m_mountBookmarkItem->setOpacity(0.3);
        }

    } else {
        m_mountBookmarkItem->hide();
    }
}

QSizeF DBookmarkItem::sizeHint(Qt::SizeHint which, const QSizeF &constraint) const
{
    Q_UNUSED(which);
    Q_UNUSED(constraint);
    return QSizeF(m_width, m_height);
}

void DBookmarkItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);

    if (!m_url.isValid() && !m_isDisk) {
        return;
    }

    if (m_url.isTaggedFile()) {

        if (static_cast<bool>(m_group) && m_pressed) {
            emit clicked();
            m_group->deselectAll();
            setChecked(true);
            m_pressed = false;
            update();

            DFMEvent fmEvent{this};
            fmEvent.setWindowId(this->windowId());
            fmEvent.setData(m_url);
            emit m_group->url(fmEvent);
        }

    } else {


        if (m_group && m_pressed) {
            emit clicked();

            if (m_group) {
                m_group->deselectAll();
                setChecked(true);
            }
            m_pressed = false;
            update();

            QDir dir(m_url.path());

            qDebug() << FileUtils::isFileExists(m_url.path());

            if (!dir.exists() && !m_isDefault) {
                qDebug() << this << m_bookmarkModel->getDevcieId();

                DUrl deviceUrl(m_bookmarkModel->getDevcieId());

                qDebug() << m_bookmarkModel->getDevcieId() << deviceUrl << NetworkManager::SupportScheme.contains(deviceUrl.scheme());

                if (NetworkManager::SupportScheme.contains(deviceUrl.scheme())) {
                    setMountBookmark(true);
                    emit fileSignalManager->requestFetchNetworks(DFMUrlBaseEvent(this, deviceUrl));
                    return;
                }

                /*handle bookmark of luks device when device is unmounted*/
                if (m_bookmarkModel->getDevcieId().startsWith("/dev/dm")) {
                    QString uuid = m_bookmarkModel->getUuid();
                    UDiskDeviceInfoPointer pDevice = deviceListener->getDeviceByUUID(uuid);
                    qDebug() << "uuid:" << uuid;
                    qDebug() << "device" << pDevice;
                    if (pDevice) {
                        setMountBookmark(true);
                        deviceListener->mount(pDevice->getDiskInfo().id());
                        return;
                    }
                }

                deviceListener->mount(m_bookmarkModel->getDevcieId());
                setMountBookmark(true);

                TIMER_SINGLESHOT(0, {

                    QDir dir(this->m_url.path());
                    if (!dir.exists())
                    {
                        DFMUrlBaseEvent event(this, this->m_url);
                        event.setWindowId(windowId());

                        int result = dialogManager->showRemoveBookMarkDialog(event);
                        if (result == DDialog::Accepted) {
                            emit fileSignalManager->requestBookmarkRemove(event);
                        }
                    }
                }, this)
            } else {
                DFMEvent e(this);
                e.setWindowId(windowId());
                if (m_url.isBookMarkFile()) {
                    e.setData(DUrl::fromLocalFile(m_url.path()));
                } else {
                    e.setData(m_url);
                }

                if (m_isDisk) {
                    if (m_deviceInfo) {
                        QDiskInfo info = m_deviceInfo->getDiskInfo();
                        qDebug() << info << m_url << m_isMounted;
                        if (!m_isMounted) {
                            m_url.setQuery(info.id());
                            appController->actionOpenDisk(dMakeEventPointer<DFMUrlBaseEvent>(this, m_url));
                        } else {
                            qDebug() << e;
                            emit m_group->url(e);
                        }
                    }
                } else {
                    emit m_group->url(e);
                }
                scene()->views().at(0)->ensureVisible(this, -10, 0);
            }
        }
    }

}

void DBookmarkItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    emit doubleClicked();
}

void DBookmarkItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    m_hovered = true;
    event->accept();
    emit dragEntered();
    update();
}

void DBookmarkItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
    m_hovered = false;
    emit dragLeft();
    QGraphicsItem::dragLeaveEvent(event);
    update();
}

void DBookmarkItem::dragMoveEvent(QGraphicsSceneDragDropEvent *event)
{
    m_hovered = true;
    event->accept();
}

void DBookmarkItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    event->acceptProposedAction();
    m_hovered = false;
    update();
    emit dropped();
    if (!event->mimeData()->hasUrls()) {
        return;
    }
    if (m_isDisk) {
        return;
    }

    DUrlList urlList;
#ifdef DDE_COMPUTER_TRASH
    //filter out trash desktop file and computer desktop file on desktop path
    foreach (const DUrl &url, DUrl::fromQUrlList(event->mimeData()->urls())) {
        if (url == DesktopFileInfo::trashDesktopFileUrl() || url == DesktopFileInfo::computerDesktopFileUrl()) {

            //prevent user unexpectedly creating new desktop file named as dde-trash/dde-computer on desktop,without specified deepinId
            DesktopFile df(url.toLocalFile());
            if (df.getDeepinId() == "dde-trash" || df.getDeepinId() == "dde-computer") {
                continue;
            }
        }
        urlList << url;
    }

    if (urlList.count() == 0) {
        return;
    }
#else
    urlList = DUrl::fromQUrlList(event->mimeData()->urls());
#endif

    if (m_url == DUrl::fromTrashFile("/")) {
        fileService->pasteFile(this, DFMGlobal::CutAction, m_url, urlList);
    } else {
        fileService->pasteFile(this, DFMGlobal::CopyAction, m_url, urlList);
    }
    QGraphicsItem::dropEvent(event);
}

bool DBookmarkItem::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lineEdit) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *key_event = static_cast<QKeyEvent *>(event);

            switch (key_event->key()) {
            case Qt::Key_Escape: {
                m_widget->deleteLater();
                m_lineEdit = Q_NULLPTR;
                break;
            }
            default: {
                break;
            }
            }
        }
    }

    return false;
}

void DBookmarkItem::keyPressEvent(QKeyEvent *event)
{
    QGraphicsItem::keyPressEvent(event);
}

void DBookmarkItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    event->accept();
    if (m_checked) {

    } else {
        m_hovered = true;
        update();
    }
}

void DBookmarkItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    Q_UNUSED(event);
    event->accept();
    if (!m_isMenuOpened) {
        m_hovered = false;
        update();
    }
}

void DBookmarkItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    Q_UNUSED(event);
    m_isMenuOpened = true;
    DFileMenu *menu = 0;


    QSet<MenuAction> disableList;

    const bool tabAddable = WindowManager::tabAddableByWinId(windowId());
    if (!tabAddable || (m_url.isLocalFile() && !QFile::exists(m_url.toLocalFile()))) {
        disableList << MenuAction::OpenInNewTab;
    }

    if (m_url.isRecentFile()) {
        menu = DFileMenuManager::createRecentLeftBarMenu(disableList);
    } else if (m_url.isTrashFile()) {
        menu = DFileMenuManager::createTrashLeftBarMenu(disableList);
    } else if (m_url.isComputerFile()) {
        menu = DFileMenuManager::createComputerLeftBarMenu(disableList);
    } else if (m_isDisk && m_deviceInfo) {
        if (!tabAddable) {
            disableList << MenuAction::OpenDiskInNewTab;
        }

        disableList |= m_deviceInfo->disableMenuActionList() ;
        m_url.setQuery(m_deviceID);

        menu = DFileMenuManager::genereteMenuByKeys(
                   m_deviceInfo->menuActionList(DAbstractFileInfo::SingleFile),
                   disableList);
    } else if (m_url.isNetWorkFile()) {
        menu = DFileMenuManager::createNetworkMarkMenu(disableList);
    } else if (m_url.isUserShareFile()) {
        menu = DFileMenuManager::createUserShareMarkMenu(disableList);
    } else if (PluginManager::instance()->getViewInterfaceByScheme(m_url.scheme())) {
        menu = DFileMenuManager::createPluginBookMarkMenu(disableList);
    } else if (m_isDefault) {
        menu = DFileMenuManager::createDefaultBookMarkMenu(disableList);

        ///###: tag protocol.
    } else if (m_url.isTaggedFile()) {
        menu = DFileMenuManager::createTagMarkMenu(disableList);

    } else {
        menu = DFileMenuManager::createCustomBookMarkMenu(m_url, disableList);
    }
    QPointer<DBookmarkItem> me = this;
    if (menu && !menu->actions().isEmpty()) {
        menu->setEventData(DUrl(), DUrlList() << m_url, windowId(), this);
        menu->exec();
        menu->deleteLater();

        if (me) {
            m_hovered = false;
        }
    }

    if (me) {
        m_isMenuOpened = false;
        update();
    }
}

void DBookmarkItem::setDraggable(bool b)
{
    m_isDraggable = b;
}

void DBookmarkItem::setBounds(int x, int y, int w, int h)
{
    m_x_axis = x;
    m_y_axis = y;
    m_width = w;
    m_height = h;
}

//void DBookmarkItem::setPressBackgroundColor(const QColor &color)
//{
////    m_pressBackgroundColor = color;
//}

//void DBookmarkItem::setReleaseBackgroundColor(const QColor &color)
//{
////   m_releaseBackgroundColor = color;
//}

//void DBookmarkItem::setHoverBackgroundColor(const QColor &color)
//{
////    m_hoverBackgroundColor = color;
//}

//void DBookmarkItem::setHighlightDiskBackgroundColor(const QColor &color)
//{
////    m_highlightDiskBackgroundColor = color;
//}

void DBookmarkItem::setHoverEnableFlag(bool flag)
{
    setAcceptHoverEvents(flag);
}

void DBookmarkItem::setAdjust(qreal value)
{
    m_adjust = value;
}

bool DBookmarkItem::isPressed()
{
    return m_pressed;
}

bool DBookmarkItem::isHovered()
{
    return m_hovered;
}

void DBookmarkItem::setHovered(bool flag)
{
    m_hovered = flag;
}

void DBookmarkItem::setBackgroundEnable(bool flag)
{
    m_backgroundEnabled = flag;
}

int DBookmarkItem::boundX()
{
    return m_x_axis;
}

int DBookmarkItem::boundY()
{
    return m_y_axis;
}

int DBookmarkItem::boundWidth()
{
    return m_width;
}

int DBookmarkItem::boundHeight()
{
    return m_height;
}

void DBookmarkItem::setText(const QString &text)
{
    m_textContent = text;
    update();
}

QString DBookmarkItem::text()
{
    return m_textContent;
}

void DBookmarkItem::setPress(bool b)
{
    m_pressed = b;
}

void DBookmarkItem::setPressBackgroundEnable(bool b)
{
    m_pressBackgroundEnabled = b;
}

void DBookmarkItem::setReleaseBackgroundEnable(bool b)
{
    m_releaseBackgroundEnabled = b;
}

void DBookmarkItem::setHoverBackgroundEnable(bool b)
{
    m_hoverBackgroundEnabled = b;
}

void DBookmarkItem::setHighlightDiskBackgroundEnable(bool b)
{
    m_highlightDiskBackgroundEnabled = b;
}

void DBookmarkItem::setHighlightDisk(bool isHighlight)
{
    if (m_isHighlightDisk == isHighlight) {
        return;
    }

    m_isHighlightDisk = isHighlight;
    if (!m_checked) {
        update();
    }
}

bool DBookmarkItem::isHighlightDisk()
{
    return m_isHighlightDisk;
}

void DBookmarkItem::setUrl(const DUrl &url)
{
    m_url = url;
}

DUrl DBookmarkItem::getUrl()
{
    return m_url;
}

void DBookmarkItem::setCheckable(bool b)
{
    m_checkable = b;
}

void DBookmarkItem::setChecked(bool b)
{
    if (m_checked == b) {
        return;
    }

    m_checked = b;
    if (m_mountBookmarkItem) {
        m_mountBookmarkItem->setChecked(b);
    }

    TIMER_SINGLESHOT(0, update(), this)
}

bool DBookmarkItem::isChecked()
{
    return m_checked;
}

void DBookmarkItem::setItemGroup(DBookmarkItemGroup *group)
{
    m_group = group;
}

void DBookmarkItem::setWidth(double w)
{
    m_width = w;
}

void DBookmarkItem::setHeight(double h)
{
    m_height = h;
}

void DBookmarkItem::setDefaultItem(bool v)
{
    m_isDefault = v;
}

bool DBookmarkItem::isDefaultItem()
{
    return m_isDefault;
}

bool DBookmarkItem::isDiskItem()
{
    return m_isDisk;
}
