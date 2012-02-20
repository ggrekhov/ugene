/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2012 UniPro <ugene@unipro.ru>
 * http://ugene.unipro.ru
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "AssemblyReadsArea.h"

#include <assert.h>
#include <math.h>

#include <QtGui/QVBoxLayout>
#include <QtGui/QPainter>
#include <QtGui/QCursor>
#include <QtGui/QResizeEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QApplication>
#include <QtGui/QClipboard>

#include <U2Core/U2AssemblyUtils.h>
#include <U2Core/U2AlphabetUtils.h>
#include <U2Core/Counter.h>
#include <U2Core/Timer.h>
#include <U2Core/Log.h>
#include <U2Core/U2SafePoints.h>
#include <U2Core/FormatUtils.h>
#include <U2Core/BaseDocumentFormats.h>
#include <U2Core/DocumentModel.h>
#include <U2Core/IOAdapter.h>
#include <U2Core/IOAdapterUtils.h>
#include <U2Core/SaveDocumentTask.h>
#include <U2Core/DNASequenceObject.h>
#include <U2Core/AddDocumentTask.h>
#include <U2Core/U2AssemblyReadIterator.h>

#include <U2Formats/DocumentFormatUtils.h>

#include "AssemblyBrowser.h"
#include "ZoomableAssemblyOverview.h"
#include "ExportReadsDialog.h"

namespace U2 {

static const QColor backgroundColor(Qt::white);
static const QColor shadowingColor(255,255,255,200);

AssemblyReadsArea::AssemblyReadsArea(AssemblyBrowserUi * ui_, QScrollBar * hBar_, QScrollBar * vBar_)
    :  QWidget(ui_), ui(ui_), browser(ui_->getWindow()), model(ui_->getModel()), redraw(true), cellRenderer(NULL),
        coveredRegionsLabel(this), hBar(hBar_), vBar(vBar_), hintData(this), mover(),
        shadowingEnabled(false), shadowingData(),
        scribbling(false), currentHotkeyIndex(-1),
        readMenu(new QMenu(this))
{
    QVBoxLayout * coveredRegionsLayout = new QVBoxLayout();
    coveredRegionsLayout->addWidget(&coveredRegionsLabel);
    setLayout(coveredRegionsLayout);
    initRedraw();
    connectSlots();
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    
    coveredRegionsLabel.installEventFilter(this);
    
    createMenu();
}

void AssemblyReadsArea::createMenu() {
    copyDataAction = readMenu->addAction(tr("Copy read information to clipboard"));
    connect(copyDataAction, SIGNAL(triggered()), SLOT(sl_onCopyReadData()));

    QMenu * exportMenu = readMenu->addMenu(tr("Export"));
        exportReadAction = exportMenu->addAction("Current read");
        connect(exportReadAction, SIGNAL(triggered()), SLOT(sl_onExportRead()));

        QAction * exportVisibleReads = exportMenu->addAction("Visible reads");
        connect(exportVisibleReads, SIGNAL(triggered()), SLOT(sl_onExportReadsOnScreen()));

    QMenu * cellRendererMenu = readMenu->addMenu(tr("Reads highlighting"));
    {
        QList<AssemblyCellRendererFactory*> factories = browser->getCellRendererRegistry()->getFactories();

        AssemblyCellRendererFactory * selectedFactory = factories.first();
        cellRenderer.reset(selectedFactory->create());

        foreach(AssemblyCellRendererFactory *f, factories) {
            QAction * action = cellRendererMenu->addAction(f->getName());
            action->setCheckable(true);
            action->setChecked(f == selectedFactory);
            action->setData(f->getId());
            connect(action, SIGNAL(triggered()), SLOT(sl_changeCellRenderer()));
            cellRendererActions << action;
        }
    }

    QMenu *shadowingMenu = createShadowingMenu();
    readMenu->addMenu(shadowingMenu);
}

static const QString BIND_HERE(AssemblyReadsArea::tr("Lock here"));

QMenu* AssemblyReadsArea::createShadowingMenu() {
    QMenu *shadowingMenu = new QMenu(tr("Reads shadowing"));

    disableShadowing = shadowingMenu->addAction(tr("Disabled"));
    disableShadowing->setCheckable(true);
    shadowingModeFree = shadowingMenu->addAction(tr("Free"));
    shadowingModeFree ->setCheckable(true);
    shadowingModeCentered = shadowingMenu->addAction(tr("Centered"));
    shadowingModeCentered->setCheckable(true);

    shadowingMenu->addSeparator();
    shadowingBindHere = shadowingMenu->addAction(BIND_HERE);
    shadowingBindHere->setDisabled(true);
    connect(shadowingBindHere, SIGNAL(triggered()), this, SLOT(sl_onBindShadowing()));

    shadowingJump = shadowingMenu->addAction(tr("Jump to locked base"));
    shadowingJump->setEnabled(false);
    connect(shadowingJump, SIGNAL(triggered()), this, SLOT(sl_onShadowingJump()));


    QActionGroup *modeGroup = new QActionGroup(this);
    modeGroup->addAction(disableShadowing);
    modeGroup->addAction(shadowingModeFree);
    modeGroup->addAction(shadowingModeCentered);

    modeGroup->setExclusive(true);
    disableShadowing->setChecked(true);

    connect(modeGroup, SIGNAL(triggered(QAction*)), this, SLOT(sl_onShadowingModeChanged(QAction*)));

    return shadowingMenu;
}

void AssemblyReadsArea::initRedraw() {
    redraw = true;
    cachedView = QPixmap(size());
}

void AssemblyReadsArea::connectSlots() {
    connect(browser, SIGNAL(si_zoomOperationPerformed()), SLOT(sl_zoomOperationPerformed()));
    connect(browser, SIGNAL(si_offsetsChanged()), SLOT(sl_redraw()));
    connect(&coveredRegionsLabel, SIGNAL(linkActivated(const QString&)), SLOT(sl_coveredRegionClicked(const QString&)));
}   

void AssemblyReadsArea::setupHScrollBar() {
    U2OpStatusImpl status;
    hBar->disconnect(this);

    qint64 assemblyLen = model->getModelLength(status);
    qint64 numVisibleBases = browser->basesVisible();

    hBar->setMinimum(0);
    hBar->setMaximum(assemblyLen - numVisibleBases);
    hBar->setSliderPosition(browser->getXOffsetInAssembly());

    hBar->setSingleStep(1);
    hBar->setPageStep(numVisibleBases);

    hBar->setDisabled(numVisibleBases == assemblyLen);

    connect(hBar, SIGNAL(valueChanged(int)), SLOT(sl_onHScrollMoved(int)));
}

void AssemblyReadsArea::setupVScrollBar() {
    U2OpStatusImpl status;
    vBar->disconnect(this);

    qint64 assemblyHeight = model->getModelHeight(status);
    qint64 numVisibleRows = browser->rowsVisible();

    vBar->setMinimum(0);
    vBar->setMaximum(assemblyHeight - numVisibleRows + 2); //TODO: remove +2
    vBar->setSliderPosition(browser->getYOffsetInAssembly());

    vBar->setSingleStep(1);
    vBar->setPageStep(numVisibleRows);

    if(numVisibleRows == assemblyHeight) {
        vBar->setDisabled(true);
        //vBar->hide(); TODO: do hide(), but prevent infinite resizing (hide->show->hide->show) caused by width change
    } else {
        vBar->setDisabled(false);
        //vBar->show();
    }

    connect(vBar, SIGNAL(valueChanged(int)), SLOT(sl_onVScrollMoved(int)));
}

void AssemblyReadsArea::drawAll() {
    GTIMER(c1, t1, "AssemblyReadsArea::drawAll");
    if(!model->isEmpty()) {
        if (redraw) {
            // update cached pixmap
            redraw = false;

            if(browser->areReadsVisible()) {
                cachedView.fill(Qt::transparent);
                QPainter p(&cachedView);
                drawReads(p);
            }
            else {
                QPainter p(this);
                p.fillRect(0, 0, width(), height(), Qt::transparent);
                showWelcomeScreen();
            }

            setupHScrollBar(); 
            setupVScrollBar();
        }

        if(browser->areReadsVisible()) {
            QPainter p(this);
            p.drawPixmap(0, 0, cachedView);

            // draw all light stuff which is faster
            // to redraw than cache into pixmap

            if(hintData.updateHint) {
                hintData.updateHint = false;
                updateHint();
            }

            drawReadsShadowing(p);
            drawCurrentReadHighlight(p);
        }
    }
}

const static QString ZOOM_LINK("zoom");

const QList<AssemblyReadsArea::HotkeyDescription> AssemblyReadsArea::HOTKEY_DESCRIPTIONS = AssemblyReadsArea::initHotkeyDescriptions();

QList<AssemblyReadsArea::HotkeyDescription> AssemblyReadsArea::initHotkeyDescriptions() {
    QList<HotkeyDescription> res;
    res << HotkeyDescription(tr("Shift+move mouse"), tr("Zoom the Assembly Overview to selection"));
    res << HotkeyDescription(tr("Ctrl+wheel"), tr("Zoom the Assembly Overview"));
    res << HotkeyDescription(tr("Alt+click"), tr("Zoom the Assembly Overview in 100x"));
    res << HotkeyDescription(tr("Wheel+move mouse"), tr("Move the Assembly Overview"));
    res << HotkeyDescription(tr("Wheel"), tr("Zoom the Reads Area"));
    res << HotkeyDescription(tr("Double-click"), tr("Zoom in the Reads Area"));
    res << HotkeyDescription(tr("+/-"), tr("Zoom in/Zoom out the Reads Area"));
    res << HotkeyDescription(tr("Click+move mouse"), tr("Move the Reads Area"));
    res << HotkeyDescription(tr("Arrow"), tr("Move one base in the corresponding direction in the Reads Area"));
    res << HotkeyDescription(tr("Ctrl+arrow"), tr("Move one page in the corresponding direction in the Reads Area"));
    res << HotkeyDescription(tr("Page up/Page down"), tr("Move one page up/down in the Reads Area"));
    res << HotkeyDescription(tr("Home/End"), tr("Move to the beginning/end of the assembly in the Reads Area"));
    res << HotkeyDescription(tr("Ctrl+G"), tr("Focus to the <i>Go to position</i> field on the toolbar"));
    return res;
}

void AssemblyReadsArea::showWelcomeScreen() {
    GTIMER(c1, t1, "AssemblyReadsArea::showWelcomeScreen");

    cachedReads.clear();
    QString text = tr("<a href=\"%1\">Zoom in to see the reads</a>").arg(ZOOM_LINK);

    QList<CoveredRegion> coveredRegions = browser->getCoveredRegions();
    if(!browser->areCoveredRegionsReady()) {
        text = tr("Please wait until overview rendering is finished, or <a href=\"%1\">zoom in to see the reads</a>").arg(ZOOM_LINK);
    } else if(!coveredRegions.empty()) {
        text += tr(" or choose one of the well-covered regions:<br><br>");
        QString coveredRegionsText = "<table align=\"center\" cellspacing=\"2\">";
        /*
        * |   | Region | Coverage |
        * | 1 | [x,y]  | z        |
        */
        coveredRegionsText += tr("<tr><td></td><td>Region</td><td>Approx.&nbsp;coverage</td></tr>");
        for(int i = 0; i < coveredRegions.size(); ++i) {
            const CoveredRegion & cr = coveredRegions.at(i);
            QString crStart = FormatUtils::splitThousands(cr.region.startPos);
            QString crEnd = FormatUtils::splitThousands(cr.region.endPos());
            QString crCoverage = FormatUtils::splitThousands(cr.coverage);
            coveredRegionsText += "<tr>";
            coveredRegionsText += QString("<td align=\"right\">%1&nbsp;&nbsp;</td>").arg(i+1);
            coveredRegionsText += QString("<td><a href=\"%1\">[%2 - %3]</a></td>").arg(i).arg(crStart).arg(crEnd);
            coveredRegionsText += tr("<td align=\"center\">%4</td>").arg(crCoverage);
            coveredRegionsText += "</tr>";
        }
        coveredRegionsText += "</table>";
        text += coveredRegionsText;
    }
    
    assert(!HOTKEY_DESCRIPTIONS.isEmpty());
    if(currentHotkeyIndex == -1 || !coveredRegionsLabel.isVisible()) {
        currentHotkeyIndex = qrand() % HOTKEY_DESCRIPTIONS.size();
    }
    text += "<br><br><br><u>TIP:</u>&nbsp;&nbsp;&nbsp;";
    HotkeyDescription hotkey = HOTKEY_DESCRIPTIONS.at(currentHotkeyIndex);
    text += QString("<b>%1</b>&nbsp;&mdash;&nbsp;%2").arg(hotkey.key).arg(hotkey.desc);
    
    coveredRegionsLabel.setText(text);
    coveredRegionsLabel.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    coveredRegionsLabel.show();

    //p.drawText(rect(), Qt::AlignCenter, );
}

void AssemblyReadsArea::drawReads(QPainter & p) {
    GTIMER(c1, t1, "AssemblyReadsArea::drawReads");
    coveredRegionsLabel.hide();

    p.setFont(browser->getFont());
    p.fillRect(rect(), backgroundColor);

    cachedReads.xOffsetInAssembly = browser->getXOffsetInAssembly();
    cachedReads.yOffsetInAssembly = browser->getYOffsetInAssembly();

    cachedReads.visibleBases = U2Region(cachedReads.xOffsetInAssembly, browser->basesCanBeVisible());
    cachedReads.visibleRows = U2Region(cachedReads.yOffsetInAssembly, browser->rowsCanBeVisible());

    // 0. Get reads from the database
    U2OpStatusImpl status;
    qint64 t = GTimer::currentTimeMicros();
    cachedReads.data = model->getReadsFromAssembly(cachedReads.visibleBases, cachedReads.visibleRows.startPos, 
        cachedReads.visibleRows.endPos(), status);
    t = GTimer::currentTimeMicros() - t;
    perfLog.trace(QString("Assembly: reads 2D load time: %1").arg(double(t) / 1000 / 1000));
    if (status.hasError()) {
        LOG_OP(status);
        return;
    }

    QByteArray referenceRegion;
    if(model->hasReference() && browser->areCellsVisible()) {
        U2OpStatusImpl status;
        referenceRegion = model->getReferenceRegion(cachedReads.visibleBases, status);
        if(status.hasError()) {
            LOG_OP(status);
            referenceRegion = QByteArray();
        }
    }

    // 1. Render cells using AssemblyCellRenderer
    cachedReads.letterWidth = browser->getCellWidth();

    bool text = browser->areLettersVisible(); 
    if(browser->areCellsVisible()) {
        GTIMER(c3, t3, "AssemblyReadsArea::drawReads -> cells rendering");
        QFont f = browser->getFont();
        if(text) {
            f.setPointSize(calcFontPointSize());
        }
        cellRenderer->render(QSize(cachedReads.letterWidth, cachedReads.letterWidth), text, f);
    }

    // 2. Iterate over all visible reads and draw them
    QListIterator<U2AssemblyRead> it(cachedReads.data);
    while(it.hasNext()) {
        GTIMER(c3, t3, "AssemblyReadsArea::drawReads -> cycle through all reads");

        const U2AssemblyRead & read = it.next();
        QByteArray readSequence = read->readSequence;
        U2Region readBases(read->leftmostPos, U2AssemblyUtils::getEffectiveReadLength(read));

        U2Region readVisibleBases = readBases.intersect(cachedReads.visibleBases);
        U2Region xToDrawRegion(readVisibleBases.startPos - cachedReads.xOffsetInAssembly, readVisibleBases.length);
        if(readVisibleBases.isEmpty()) {
            continue;
        }

        U2Region readVisibleRows = U2Region(read->packedViewRow, 1).intersect(cachedReads.visibleRows); // WTF?
        U2Region yToDrawRegion(readVisibleRows.startPos - cachedReads.yOffsetInAssembly, readVisibleRows.length);
        if(readVisibleRows.isEmpty()) {
            continue;
        }

        if(browser->areCellsVisible()) { //->draw color rects
            int firstVisibleBase = readVisibleBases.startPos - readBases.startPos; 
            int x_pix_start = browser->calcPainterOffset(xToDrawRegion.startPos);
            int y_pix_start = browser->calcPainterOffset(yToDrawRegion.startPos);

            //iterate over letters of the read
            QList<U2CigarToken> cigar(read->cigar); // hack: to show reads without cigar but with mapped position
            if(cigar.isEmpty()) {
                cigar << U2CigarToken(U2CigarOp_M, readSequence.size());
            }
            U2AssemblyReadIterator cigarIt(readSequence, cigar, firstVisibleBase);
            int basesPainted = 0;
            for(int x_pix_offset = 0; cigarIt.hasNext() && basesPainted++ < readVisibleBases.length; x_pix_offset += cachedReads.letterWidth) {
                GTIMER(c2, t2, "AssemblyReadsArea::drawReads -> cycle through one read");
                char c = cigarIt.nextLetter();

                QPoint cellStart(x_pix_start + x_pix_offset, y_pix_start);
                QPixmap cellImage;
                if(! referenceRegion.isEmpty()) {
                    int posInRef = readVisibleBases.startPos - cachedReads.visibleBases.startPos + basesPainted - 1;
                    cellImage = cellRenderer->cellImage(read, c, referenceRegion[posInRef]);
                } else {
                    cellImage = cellRenderer->cellImage(read, c);
                }

                p.drawPixmap(cellStart, cellImage);
            }
        } else { 
            int xstart = browser->calcPixelCoord(xToDrawRegion.startPos);
            int xend = browser->calcPixelCoord(xToDrawRegion.endPos());
            int ystart = browser->calcPixelCoord(yToDrawRegion.startPos);
            int yend = browser->calcPixelCoord(yToDrawRegion.endPos());

            p.fillRect(xstart, ystart, xend - xstart, yend - ystart, Qt::black);
        }
    }    
}

bool AssemblyReadsArea::findReadOnPos(const QPoint &pos, U2AssemblyRead &read) {
    qint64 asmX = cachedReads.xOffsetInAssembly + (double)pos.x() / cachedReads.letterWidth;
    qint64 asmY = cachedReads.yOffsetInAssembly + (double)pos.y() / cachedReads.letterWidth;
    bool found = false;
    QListIterator<U2AssemblyRead> it(cachedReads.data);
    while(it.hasNext()) {
        const U2AssemblyRead & r = it.next();
        if(r->packedViewRow == asmY && asmX >= r->leftmostPos && asmX < r->leftmostPos + U2AssemblyUtils::getEffectiveReadLength(r)) {
            read = r;
            found = true;
            break;
        }
    }
    return found;
}

QList<U2AssemblyRead> AssemblyReadsArea::findReadsCrossingX(qint64 asmX) {
    QList<U2AssemblyRead> found;
    foreach (const U2AssemblyRead &r, cachedReads.data) {
        if(asmX >= r->leftmostPos && asmX < r->leftmostPos + U2AssemblyUtils::getEffectiveReadLength(r)) {
            found << r;
        }
    }

    return found;
}

void AssemblyReadsArea::updateHint() {
    if(cachedReads.isEmpty() || cachedReads.letterWidth == 0 || scribbling) {
        sl_hideHint();
        return;
    }

    // 1. find assembly read we stay on
    U2AssemblyRead read;
    bool found = findReadOnPos(curPos, read);
    if(!found) {
        sl_hideHint();
        return;
    }
    
    // 2. set hint info
    if(read->id != hintData.curReadId) {
        hintData.curReadId = read->id;

        // find mates
        U2OpStatusImpl status;
        QList<U2AssemblyRead> mates = model->findMateReads(read, status);
        if(status.hasError()) {
            LOG_OP(status);
            mates = QList<U2AssemblyRead>();
        }

        // set data
        hintData.hint.setData(read, mates);
    }

    // 3. move hint if needed
    QRect readsAreaRect(mapToGlobal(rect().topLeft()), mapToGlobal(rect().bottomRight()));
    QRect hintRect = hintData.hint.rect(); 
    hintRect.moveTo(QCursor::pos() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR);
    QPoint offset(0, 0);
    if(hintRect.right() > readsAreaRect.right()) {
        offset -= QPoint(hintRect.right() - readsAreaRect.right(), 0);
    }
    if(hintRect.bottom() > readsAreaRect.bottom()) {
        offset -= QPoint(0, hintRect.bottom() - readsAreaRect.bottom()); // move hint bottom to reads area
        offset -= QPoint(0, readsAreaRect.bottom() - QCursor::pos().y() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR.y());
    }
    QPoint newPos = QCursor::pos() + AssemblyReadsAreaHint::OFFSET_FROM_CURSOR + offset;
    if(hintData.hint.pos() != newPos) {
        hintData.hint.move(newPos);
    }
    if(!hintData.hint.isVisible()) {
        hintData.hint.show();
    }
}

QRect AssemblyReadsArea::calcReadRect(const U2AssemblyRead &read) {
    U2Region readBases(read->leftmostPos, U2AssemblyUtils::getEffectiveReadLength(read));
    U2Region readVisibleBases = readBases.intersect(cachedReads.visibleBases);
    assert(!readVisibleBases.isEmpty());
    U2Region xToDrawRegion(readVisibleBases.startPos - cachedReads.xOffsetInAssembly, readVisibleBases.length);

    U2Region readVisibleRows = U2Region(read->packedViewRow, 1).intersect(cachedReads.visibleRows);
    U2Region yToDrawRegion(readVisibleRows.startPos - cachedReads.yOffsetInAssembly, readVisibleRows.length);
    assert(!readVisibleRows.isEmpty());

    assert(browser->areCellsVisible());
    int x_pix_start = browser->calcPainterOffset(xToDrawRegion.startPos);
    int y_pix_start = browser->calcPainterOffset(yToDrawRegion.startPos);

    int width = readVisibleBases.length * cachedReads.letterWidth;
    int height = cachedReads.letterWidth;

    return QRect(x_pix_start, y_pix_start, width, height);
}

void AssemblyReadsArea::drawCurrentReadHighlight(QPainter &p) {
    U2AssemblyRead read;
    bool found = findReadOnPos(curPos, read);
    if (found) {
        p.setBrush(Qt::NoBrush);
        p.setPen(Qt::darkRed);
        QRect rect = calcReadRect(read);
        rect.setBottomRight(rect.bottomRight() - QPoint(1,1));

        p.drawRect(rect);
    }
}

void AssemblyReadsArea::drawReadsShadowing(QPainter &p) {
     if (shadowingEnabled) {
        int screenLinePos = 0;
        qint64 asmLinePos = 0;

        if (shadowingData.mode == ShadowingData::FREE) {
            screenLinePos = curPos.x();
            asmLinePos = cachedReads.xOffsetInAssembly + (double)screenLinePos / cachedReads.letterWidth;
        }
        else if (shadowingData.mode == ShadowingData::CENTERED) {
            screenLinePos = width() / 2.0;
            asmLinePos = cachedReads.xOffsetInAssembly + (double)screenLinePos / cachedReads.letterWidth;
        }
        else if (shadowingData.mode == ShadowingData::BIND) {
            asmLinePos = shadowingData.boundPos;
            screenLinePos = (asmLinePos - cachedReads.xOffsetInAssembly) * cachedReads.letterWidth;
        }
        else {
            assert(!"Unknown ShadowingMode");
        }

        QList<U2AssemblyRead> crossedReads = findReadsCrossingX(asmLinePos);

        QVector<QRect> rects;
        if (!crossedReads.isEmpty()) {
            rects.reserve(cachedReads.data.size() - crossedReads.size());

            foreach (const U2AssemblyRead &read, cachedReads.data) {
                if (crossedReads.contains(read)) {
                    continue;
                }

                const QRect &rect = calcReadRect(read);
                rects << rect;
            }
        }
        else {
            // crossed reads are out of the screen, shadow all visible reads
            rects << QRect(this->rect());
        }

        p.setBrush(QBrush(shadowingColor));
        p.setPen(Qt::NoPen);

        p.drawRects(rects);

        if (screenLinePos >= 0 && screenLinePos < width()) {
            static const QPen cursorPen(Qt::darkRed);

            // snap cursor to letter center
            int x = (screenLinePos / cachedReads.letterWidth) * cachedReads.letterWidth + (double)cachedReads.letterWidth/2;

            p.setPen(cursorPen);
            p.drawLine(x, 0, x, height());
        }
    }
}

int AssemblyReadsArea::calcFontPointSize() const {
    return browser->getCellWidth() / 2;
}

void AssemblyReadsArea::paintEvent(QPaintEvent * e) {
    assert(model);
    drawAll();
    QWidget::paintEvent(e);
}

void AssemblyReadsArea::resizeEvent(QResizeEvent * e) {
    if(e->oldSize().height() - e->size().height()) {
        emit si_heightChanged();
    }
    initRedraw();
    QWidget::resizeEvent(e);
}

void AssemblyReadsArea::wheelEvent(QWheelEvent * e) {
    bool positive = e->delta() > 0;
    int numDegrees = abs(e->delta()) / 8;
    int numSteps = numDegrees / 15;

    // zoom
    if(Qt::NoButton == e->buttons()) {
        for(int i = 0; i < numSteps; ++i) {
            if(positive) {
                browser->sl_zoomIn(curPos);
            } else {
                browser->sl_zoomOut(curPos);
            }
        }
    }
    QWidget::wheelEvent(e);
}

void AssemblyReadsArea::mousePressEvent(QMouseEvent * e) {
    curPos = e->pos();
    if(browser->getCellWidth() != 0 && e->button() == Qt::LeftButton) {
        scribbling = true;
        setCursor(Qt::ClosedHandCursor);
        mover = ReadsMover(browser->getCellWidth(), curPos);
    }
    if(e->button() == Qt::RightButton && browser->areCellsVisible()) {
        updateMenuActions();
        readMenu->exec(QCursor::pos());
    }
    QWidget::mousePressEvent(e);
}

void AssemblyReadsArea::mouseReleaseEvent(QMouseEvent * e) {
    if(e->button() == Qt::LeftButton && scribbling) {
        scribbling = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mousePressEvent(e);
}

void AssemblyReadsArea::mouseMoveEvent(QMouseEvent * e) {
    emit si_mouseMovedToPos(e->pos());
    if((e->buttons() & Qt::LeftButton) && scribbling) {
        mover.handleEvent(e->pos());
        int x_units = mover.getXunits();
        int y_units = mover.getYunits();
        browser->adjustOffsets(-x_units, -y_units);
    }
    curPos = e->pos();
    hintData.updateHint = true;
    update();
    QWidget::mouseMoveEvent(e);
}

void AssemblyReadsArea::leaveEvent(QEvent *) {
    QPoint curInHintCoords = hintData.hint.mapFromGlobal(QCursor::pos());
    if(!hintData.hint.rect().contains(curInHintCoords)) {
        sl_hideHint();
    }
}

void AssemblyReadsArea::hideEvent(QHideEvent *) {
    sl_hideHint();
}

bool AssemblyReadsArea::event(QEvent * e) {
    QEvent::Type t = e->type();
    if(t == QEvent::WindowDeactivate) {
        sl_hideHint();
        hintData.updateHint = false;
    }
    return QWidget::event(e);
}

void AssemblyReadsArea::keyPressEvent(QKeyEvent * e) {
    int k = e->key();
    if(browser->getCellWidth() != 0 && (k == Qt::Key_Left || k == Qt::Key_Right)) {
        if(hBar->isEnabled()) {
            int step = e->modifiers() & Qt::ControlModifier ? hBar->pageStep() : hBar->singleStep();
            step = k == Qt::Key_Left ? -step : step;
            hBar->setValue(hBar->value() + step);
            e->accept();
        }
    } else if(k == Qt::Key_Up || k == Qt::Key_Down) {
        if(vBar->isEnabled()) {
            int step = e->modifiers() & Qt::ControlModifier ? vBar->pageStep() : vBar->singleStep();
            step = k == Qt::Key_Up ? -step : step;
            vBar->setValue(vBar->value() + step);
            e->accept();    
        }
    } else if(k == Qt::Key_Home) {
        if(hBar->isEnabled()) {
            hBar->setValue(0);
            e->accept();
        }
    } else if(k == Qt::Key_End) {
        if(hBar->isEnabled()) {
            U2OpStatusImpl status;
            hBar->setValue(model->getModelLength(status));
            LOG_OP(status);
            e->accept();
        }
    } else if(k == Qt::Key_Plus) {
        browser->sl_zoomIn();
        e->accept();
    } else if(k == Qt::Key_Minus) {
        browser->sl_zoomOut();
        e->accept();
    } else if(k == Qt::Key_G && (e->modifiers() & Qt::ControlModifier)) {
        browser->setFocusToPosSelector();
        e->accept();
    } else if(k == Qt::Key_PageUp || k == Qt::Key_PageDown) {
        if(vBar->isEnabled()) {
            int step = k == Qt::Key_PageUp ? -vBar->pageStep() : vBar->pageStep();
            vBar->setValue(vBar->value() + step);
            e->accept();
        }
    }

    if(!e->isAccepted()) {
        QWidget::keyPressEvent(e);
    }
}

bool AssemblyReadsArea::eventFilter(QObject *obj, QEvent *ev) {
    if(obj == &coveredRegionsLabel) {
        if(ev->type() == QEvent::MouseMove) {
            QWidget::event(ev);
        }
        return false;
    } else {
        return QWidget::eventFilter(obj, ev);
    }
}

void AssemblyReadsArea::mouseDoubleClickEvent(QMouseEvent * e) {
    qint64 cursorXoffset = browser->calcAsmPosX(e->pos().x());
    qint64 cursorYoffset = browser->calcAsmPosY(e->pos().y());

    //1. zoom in
    static const int howManyZoom = 1;
    for(int i = 0; i < howManyZoom; ++i) {
        browser->sl_zoomIn();
    }

    //2. move cursor offset to center
    // move x
    if(hBar->isEnabled()) {
        qint64 xOffset = browser->getXOffsetInAssembly();
        qint64 windowHalfX = xOffset + qRound64((double)browser->basesCanBeVisible() / 2);
        browser->setXOffsetInAssembly(browser->normalizeXoffset(xOffset + cursorXoffset - windowHalfX + 1));
    }
    // move y
    if(vBar->isEnabled()) {
        qint64 yOffset = browser->getYOffsetInAssembly();
        qint64 windowHalfY = yOffset + qRound64((double)browser->rowsCanBeVisible() / 2);
        browser->setYOffsetInAssembly(browser->normalizeYoffset(yOffset + cursorYoffset - windowHalfY + 1));
    }
}

void AssemblyReadsArea::sl_coveredRegionClicked(const QString & link) {
    if(ZOOM_LINK == link) {
        browser->sl_zoomToReads();
    } else {
        bool ok;
        int i = link.toInt(&ok);
        assert(ok);
        CoveredRegion cr = browser->getCoveredRegions().at(i);
        ui->getOverview()->checkedSetVisibleRange(cr.region);
        browser->navigateToRegion(ui->getOverview()->getVisibleRange());
    }
}

void AssemblyReadsArea::sl_onHScrollMoved(int pos) {
    browser->setXOffsetInAssembly(pos);
}

void AssemblyReadsArea::sl_onVScrollMoved(int pos) {
    browser->setYOffsetInAssembly(pos);
}

void AssemblyReadsArea::sl_zoomOperationPerformed() {
    sl_redraw();
}

void AssemblyReadsArea::sl_redraw() {
    initRedraw();
    update();
}

void AssemblyReadsArea::sl_hideHint() {
    hintData.hint.hide();
    update();
}

void AssemblyReadsArea::sl_onCopyReadData() {
    U2AssemblyRead read;
    bool found = findReadOnPos(curPos, read);
    if(!found) {
        return;
    }
    QApplication::clipboard()->setText(AssemblyReadsAreaHint::getReadDataAsString(read));
}

void AssemblyReadsArea::updateMenuActions() {
    U2AssemblyRead read;
    bool found = findReadOnPos(curPos, read);
    copyDataAction->setEnabled(found);
    exportReadAction->setEnabled(found);
}

void AssemblyReadsArea::exportReads(const QList<U2AssemblyRead> & reads) {
    GCOUNTER( cvar, tvar, "AssemblyReadsArea:exportReads" );
    
    assert(!reads.isEmpty());
    ExportReadsDialog dlg(this, QList<DocumentFormatId>() << BaseDocumentFormats::FASTA << BaseDocumentFormats::FASTQ);
    int ret = dlg.exec();
    if(ret == QDialog::Accepted) {
        ExportReadsDialogModel model = dlg.getModel();
        assert(!model.filepath.isEmpty());
        DocumentFormat * df = AppContext::getDocumentFormatRegistry()->getFormatById(model.format);
        if(df == NULL) {
            assert(false);
            return;
        }
        IOAdapterFactory * iof = AppContext::getIOAdapterRegistry()->getIOAdapterFactoryById(IOAdapterUtils::url2io(model.filepath));
        U2OpStatus2Log os;
        Document * doc = df->createNewLoadedDocument(iof, model.filepath, os);
        CHECK_OP(os, )
        SaveDocFlags fl;
        fl |= SaveDoc_Overwrite;
		fl |= SaveDoc_DestroyAfter;

        QList<GObject*> objs;
        foreach(const U2AssemblyRead & r, reads) {
            DNAAlphabet * al = U2AlphabetUtils::findBestAlphabet(r->readSequence);
            DNASequence seq = DNASequence(r->name, r->readSequence, al);
            seq.quality = DNAQuality(r->quality, DNAQualityType_Sanger);
            U2SequenceObject* seqObj = DocumentFormatUtils::addSequenceObjectDeprecated(doc->getDbiRef(), seq.getName(), objs, seq, os);
            CHECK_OP(os, );
            doc->addObject(seqObj);
        }

        SaveDocumentTask * saveDocTask = new SaveDocumentTask(doc, fl);
        Task * t = NULL;
        if (!model.addToProject) { // only saving
            t = saveDocTask;
        } else { // save, add doc
            t = new AddDocumentTask(new Document(df, iof, model.filepath, U2DbiRef())); // new doc because doc will be deleted
            t->addSubTask(saveDocTask);
            t->setMaxParallelSubtasks(1);
        }
        AppContext::getTaskScheduler()->registerTopLevelTask(t);
    }
}

void AssemblyReadsArea::sl_onExportRead() {
    U2AssemblyRead read;
    bool found = findReadOnPos(curPos, read);
    if(!found) {
        return;
    }
    exportReads(QList<U2AssemblyRead>() << read);
}

void AssemblyReadsArea::sl_onExportReadsOnScreen() {
    if(!cachedReads.data.isEmpty()) {
        exportReads(cachedReads.data);
    }
}

void AssemblyReadsArea::shadowingMenuSetBind(bool enable) {
    if (enable) {
        shadowingBindHere->setText(tr("Locked to base"));
        shadowingBindHere->setCheckable(true);
        shadowingBindHere->setChecked(true);
        shadowingJump->setEnabled(true);
    }
    else {
        shadowingBindHere->setText(BIND_HERE);
        shadowingBindHere->setCheckable(false);
        shadowingBindHere->setChecked(false);
        shadowingJump->setEnabled(false);
    }
}

void AssemblyReadsArea::sl_onShadowingModeChanged(QAction *a) {
    if (a == disableShadowing) {
        shadowingEnabled = false;
        shadowingData.mode = ShadowingData::FREE;
    }
    else {
        shadowingEnabled = true;
    }

    if (a == shadowingModeFree) {
        if (shadowingData.mode != ShadowingData::BIND) {
            shadowingData.mode = AssemblyReadsArea::ShadowingData::FREE;

            shadowingBindHere->setEnabled(true);
        }
    }
    else  {
        shadowingMenuSetBind(false);
        shadowingBindHere->setEnabled(false);
    }

    if (a == shadowingModeCentered) {
        shadowingData.mode = AssemblyReadsArea::ShadowingData::CENTERED;
    }
}

void AssemblyReadsArea::sl_onBindShadowing() {
    if (shadowingData.mode != ShadowingData::BIND) {
        shadowingData.mode = ShadowingData::BIND;
        qint64 asmX = cachedReads.xOffsetInAssembly + (double)curPos.x() / cachedReads.letterWidth;
        shadowingData.boundPos = asmX;

        shadowingMenuSetBind(true);
    }
    else {
        shadowingData.mode = ShadowingData::FREE;
        shadowingMenuSetBind(false);
    }
}

void AssemblyReadsArea::sl_onShadowingJump() {
    int onScreen = cachedReads.visibleBases.length;
    int center = onScreen / 2;
    qint64 pos = shadowingData.boundPos;
    pos = (pos < center) ? 0 : shadowingData.boundPos - center;

    U2OpStatusImpl os;
    qint64 len = model->getModelLength(os);
    pos = (pos > len - onScreen +1) ? len - onScreen +1 : pos;

    browser->setXOffsetInAssembly(pos);
}

void AssemblyReadsArea::sl_changeCellRenderer() {
    QAction * action = qobject_cast<QAction*>(sender());
    SAFE_POINT(action != NULL, "changing cell renderer invoked not by action, ignoring request",);

    QString id = action->data().toString();
    AssemblyCellRendererFactory* f = browser->getCellRendererRegistry()->getFactoryById(id);
    SAFE_POINT(f != NULL, "cannot change cell renderer, bad id",);

    cellRenderer.reset(f->create());

    foreach(QAction* a, cellRendererActions) {
        a->setChecked(a == action);
    }

    sl_redraw();
}

} //ns
