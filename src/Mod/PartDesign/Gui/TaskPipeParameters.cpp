/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
#include <QAction>
#include <QMessageBox>
#include <QMetaObject>
#endif

#include <App/Application.h>
#include <App/DocumentObject.h>
#include <App/Origin.h>
#include <Gui/CommandT.h>
#include <Gui/Document.h>
#include <Gui/MainWindow.h>
#include <Gui/Selection/Selection.h>
#include <Gui/Tools.h>
#include <Gui/ViewProvider.h>
#include <Gui/Widgets.h>
#include <Mod/PartDesign/App/Body.h>
#include <Mod/PartDesign/App/FeaturePipe.h>

#include "ui_TaskPipeParameters.h"
#include "ui_TaskPipeOrientation.h"
#include "ui_TaskPipeScaling.h"
#include <ui_DlgReference.h>

#include "TaskPipeParameters.h"
#include "TaskFeaturePick.h"
#include "TaskSketchBasedParameters.h"
#include "Utils.h"


Q_DECLARE_METATYPE(App::PropertyLinkSubList::SubSet)

using namespace PartDesignGui;
using namespace Gui;

/* TRANSLATOR PartDesignGui::TaskPipeParameters */


//**************************************************************************
//**************************************************************************
// Task Parameter
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskPipeParameters::TaskPipeParameters(ViewProviderPipe* PipeView, bool /*newObj*/, QWidget* parent)
    : TaskSketchBasedParameters(PipeView, parent, "PartDesign_AdditivePipe", tr("Pipe parameters"))
    , ui(new Ui_TaskPipeParameters)
    , stateHandler(nullptr)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    // some buttons are handled in a buttongroup
    connect(ui->buttonProfileBase,
            &QToolButton::toggled,
            this,
            &TaskPipeParameters::onProfileButton);
    connect(ui->comboBoxTransition,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPipeParameters::onTransitionChanged);

    // Create context menu
    QAction* remove = new QAction(tr("Remove"), this);
    remove->setShortcut(Gui::QtTools::deleteKeySequence());
    remove->setShortcutContext(Qt::WidgetShortcut);

    // display shortcut behind the context menu entry
    remove->setShortcutVisibleInContextMenu(true); 


    ui->listWidgetReferences->addAction(remove);
    connect(remove, &QAction::triggered, this, &TaskPipeParameters::onDeleteEdge);
    ui->listWidgetReferences->setContextMenuPolicy(Qt::ActionsContextMenu);

    this->groupLayout()->addWidget(proxy);

    PartDesign::Pipe* pipe = PipeView->getObject<PartDesign::Pipe>();
    Gui::Document* doc = PipeView->getDocument();

    // make sure the user sees all important things and load the values
    // also save visibility state to reset it later when pipe is closed
    // first the spine
    if (pipe->Spine.getValue()) {
        auto* spineVP = doc->getViewProvider(pipe->Spine.getValue());
        spineShow = spineVP->isShow();
        spineVP->setVisible(true);
        ui->spineBaseEdit->setText(QString::fromUtf8(pipe->Spine.getValue()->Label.getValue()));
    }
    // the profile
    if (pipe->Profile.getValue()) {
        auto* profileVP = doc->getViewProvider(pipe->Profile.getValue());
        profileShow = profileVP->isShow();
        profileVP->setVisible(true);
        ui->profileBaseEdit->setText(
            make2DLabel(pipe->Profile.getValue(), pipe->Profile.getSubValues()));
    }
    // the auxiliary spine
    if (pipe->AuxiliarySpine.getValue()) {
        auto* svp = doc->getViewProvider(pipe->AuxiliarySpine.getValue());
        auxSpineShow = svp->isShow();
        svp->show();
    }
    // the spine edges
    std::vector<std::string> strings = pipe->Spine.getSubValues();
    for (const auto& string : strings) {
        QString label = QString::fromStdString(string);
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(label);
        item->setData(Qt::UserRole, QByteArray(label.toUtf8()));
        ui->listWidgetReferences->addItem(item);
    }

    if (!strings.empty()) {
        PipeView->makeTemporaryVisible(true);
    }

    ui->comboBoxTransition->setCurrentIndex(pipe->Transition.getValue());

    updateUI();
    this->blockSelection(false);
}

TaskPipeParameters::~TaskPipeParameters()
{
    try {
        if (auto pipe = getObject<PartDesign::Pipe>()) {
            // setting visibility to true is needed when preselecting profile and path prior to
            // invoking sweep
            Gui::cmdGuiObject(pipe, "Visibility = True");
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Spine, false);
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Profile,
                                                                   false);
        }
    }
    catch (const Standard_OutOfRange&) {
    }
    catch (const Base::Exception& e) {
        // getDocument() may raise an exception
        e.reportException();
    }
    catch (const Py::Exception&) {
        Base::PyException e;  // extract the Python error text
        e.reportException();
    }
}

void TaskPipeParameters::updateUI()
{}

void TaskPipeParameters::onSelectionChanged(const Gui::SelectionChanges& msg)
{
    if (stateHandler->getSelectionMode() == StateHandlerTaskPipe::SelectionModes::none) {
        return;
    }

    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        if (referenceSelected(msg)) {
            if (stateHandler->getSelectionMode()
                == StateHandlerTaskPipe::SelectionModes::refProfile) {
                App::Document* document = App::GetApplication().getDocument(msg.pDocName);
                App::DocumentObject* object =
                    document ? document->getObject(msg.pObjectName) : nullptr;
                if (object) {
                    QString label = make2DLabel(object, {msg.pSubName});
                    ui->profileBaseEdit->setText(label);
                }
            }
            else if (stateHandler->getSelectionMode()
                     == StateHandlerTaskPipe::SelectionModes::refSpineEdgeAdd) {
                QString sub = QString::fromStdString(msg.pSubName);
                if (!sub.isEmpty()) {
                    QListWidgetItem* item = new QListWidgetItem();
                    item->setText(sub);
                    item->setData(Qt::UserRole, QByteArray(msg.pSubName));
                    ui->listWidgetReferences->addItem(item);
                }

                App::Document* document = App::GetApplication().getDocument(msg.pDocName);
                App::DocumentObject* object =
                    document ? document->getObject(msg.pObjectName) : nullptr;
                if (object) {
                    QString label = QString::fromUtf8(object->Label.getValue());
                    ui->spineBaseEdit->setText(label);
                }
            }
            else if (stateHandler->getSelectionMode()
                     == StateHandlerTaskPipe::SelectionModes::refSpineEdgeRemove) {
                QString sub = QString::fromLatin1(msg.pSubName);
                if (!sub.isEmpty()) {
                    removeFromListWidget(ui->listWidgetReferences, sub);
                }
                else {
                    ui->spineBaseEdit->clear();
                }
            }
            else if (stateHandler->getSelectionMode()
                     == StateHandlerTaskPipe::SelectionModes::refSpine) {
                ui->listWidgetReferences->clear();

                App::Document* document = App::GetApplication().getDocument(msg.pDocName);
                App::DocumentObject* object =
                    document ? document->getObject(msg.pObjectName) : nullptr;
                if (object) {
                    QString label = QString::fromUtf8(object->Label.getValue());
                    ui->spineBaseEdit->setText(label);
                }
            }

            clearButtons();
            recomputeFeature();
        }

        clearButtons();
        exitSelectionMode();
    }
}

void TaskPipeParameters::onTransitionChanged(int idx)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        pipe->Transition.setValue(idx);
        recomputeFeature();
    }
}

void TaskPipeParameters::onProfileButton(bool checked)
{
    if (checked) {
        if (auto pipe = getObject<PartDesign::Pipe>()) {
            Gui::Document* doc = getGuiDocument();

            if (pipe->Profile.getValue()) {
                auto* pvp = doc->getViewProvider(pipe->Profile.getValue());
                pvp->setVisible(true);
            }
        }
    }
}

void TaskPipeParameters::onTangentChanged(bool checked)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        pipe->SpineTangent.setValue(checked);
        recomputeFeature();
    }
}

void TaskPipeParameters::removeFromListWidget(QListWidget* widget, QString itemstr)
{
    QList<QListWidgetItem*> items = widget->findItems(itemstr, Qt::MatchExactly);
    if (!items.empty()) {
        for (auto item : items) {
            QListWidgetItem* it = widget->takeItem(widget->row(item));
            delete it;
        }
    }
}

void TaskPipeParameters::onDeleteEdge()
{
    // Delete the selected path edge
    int row = ui->listWidgetReferences->currentRow();
    QListWidgetItem* item = ui->listWidgetReferences->takeItem(row);
    if (item) {
        QByteArray data = item->data(Qt::UserRole).toByteArray();
        delete item;

        // search inside the list of spines
        const auto pipe = getObject<PartDesign::Pipe>();
        std::vector<std::string> refs = pipe->Spine.getSubValues();
        const std::string obj = data.constData();

        // if something was found, delete it and update the spine list
        if (const auto f = std::ranges::find(refs, obj); f != refs.end()) {
            refs.erase(f);
            pipe->Spine.setValue(pipe->Spine.getValue(), refs);
            clearButtons();
            recomputeFeature();
        }
    }
}

bool TaskPipeParameters::referenceSelected(const SelectionChanges& msg) const
{
    auto selectionMode = stateHandler->getSelectionMode();

    if (msg.Type == Gui::SelectionChanges::AddSelection
        && selectionMode != StateHandlerTaskPipe::SelectionModes::none) {
        if (strcmp(msg.pDocName, getAppDocument()->getName()) != 0) {
            return false;
        }

        // not allowed to reference ourself
        const char* fname = getObject()->getNameInDocument();
        if (strcmp(msg.pObjectName, fname) == 0) {
            return false;
        }

        switch (selectionMode) {
            case StateHandlerTaskPipe::SelectionModes::refProfile: {
                auto pipe = getObject<PartDesign::Pipe>();
                Gui::Document* doc = getGuiDocument();

                getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Profile,
                                                                       false);

                bool success = true;
                App::DocumentObject* profile = pipe->getDocument()->getObject(msg.pObjectName);
                if (profile) {
                    std::vector<App::DocumentObject*> sections = pipe->Sections.getValues();

                    // cannot use the same object for profile and section
                    if (std::ranges::find(sections, profile) != sections.end()) {
                        success = false;
                    }
                    else {
                        pipe->Profile.setValue(profile, {msg.pSubName});
                    }

                    // hide the old or new profile again
                    auto* pvp = doc->getViewProvider(pipe->Profile.getValue());
                    if (pvp) {
                        pvp->setVisible(false);
                    }
                }
                return success;
            }
            case StateHandlerTaskPipe::SelectionModes::refSpine:
            case StateHandlerTaskPipe::SelectionModes::refSpineEdgeAdd:
            case StateHandlerTaskPipe::SelectionModes::refSpineEdgeRemove: {
                // change the references
                const std::string subName(msg.pSubName);
                const auto pipe = getObject<PartDesign::Pipe>();
                std::vector<std::string> refs = pipe->Spine.getSubValues();
                const auto f = std::ranges::find(refs, subName);

                if (selectionMode == StateHandlerTaskPipe::SelectionModes::refSpine) {
                    getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Spine,
                                                                           false);
                    refs.clear();
                }
                else if (selectionMode == StateHandlerTaskPipe::SelectionModes::refSpineEdgeAdd) {
                    if (f == refs.end()) {
                        refs.push_back(subName);
                    }
                    else {
                        return false;  // duplicate selection
                    }
                }
                else if (selectionMode
                         == StateHandlerTaskPipe::SelectionModes::refSpineEdgeRemove) {
                    if (f != refs.end()) {
                        refs.erase(f);
                    }
                    else {
                        return false;
                    }
                }

                pipe->Spine.setValue(getAppDocument()->getObject(msg.pObjectName), refs);
                return true;
            }
            default:
                return false;
        }
    }

    return false;
}

void TaskPipeParameters::clearButtons()
{
    ui->buttonProfileBase->setChecked(false);
    ui->buttonRefAdd->setChecked(false);
    ui->buttonRefRemove->setChecked(false);
    ui->buttonSpineBase->setChecked(false);
}

void TaskPipeParameters::exitSelectionMode()
{
    // commenting because this should be handled by buttonToggled signal
    // selectionMode = none;
    Gui::Selection().clearSelection();
}

void TaskPipeParameters::setVisibilityOfSpineAndProfile()
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        Gui::Document* doc = getGuiDocument();

        // set visibility to the state when the pipe was opened
        for (auto obj : pipe->Sections.getValues()) {
            auto* sectionVP = doc->getViewProvider(obj);
            sectionVP->setVisible(profileShow);
        }
        if (pipe->Spine.getValue()) {
            auto* spineVP = doc->getViewProvider(pipe->Spine.getValue());
            spineVP->setVisible(spineShow);
            spineShow = false;
        }
        if (pipe->Profile.getValue()) {
            auto* profileVP = doc->getViewProvider(pipe->Profile.getValue());
            profileVP->setVisible(profileShow);
            profileShow = false;
        }
        if (pipe->AuxiliarySpine.getValue()) {
            auto* svp = doc->getViewProvider(pipe->AuxiliarySpine.getValue());
            svp->setVisible(auxSpineShow);
            auxSpineShow = false;
        }
    }
}

bool TaskPipeParameters::accept()
{
    // see what to do with external references
    // check the prerequisites for the selected objects
    // the user has to decide which option we should take if external references are used
    auto pipe = getObject<PartDesign::Pipe>();
    auto pcActiveBody = PartDesignGui::getBodyFor(pipe, false);
    if (!pcActiveBody) {
        QMessageBox::warning(this, tr("Input error"), tr("No active body"));
        return false;
    }
    // auto pcActivePart = PartDesignGui::getPartFor (pcActiveBody, false);
    std::vector<App::DocumentObject*> copies;

    bool extReference = false;
    App::DocumentObject* spine = pipe->Spine.getValue();
    App::DocumentObject* auxSpine = pipe->AuxiliarySpine.getValue();

    // If a spine isn't set but user entered a label then search for the appropriate document object
    QString label = ui->spineBaseEdit->text();
    if (!spine && !label.isEmpty()) {
        QByteArray ba = label.toUtf8();
        std::vector<App::DocumentObject*> objs =
            pipe->getDocument()->findObjects(App::DocumentObject::getClassTypeId(),
                                             nullptr,
                                             ba.constData());
        if (!objs.empty()) {
            pipe->Spine.setValue(objs.front());
            spine = objs.front();
        }
    }

    if (spine && !pcActiveBody->hasObject(spine) && !pcActiveBody->getOrigin()->hasObject(spine)) {
        extReference = true;
    }
    else if (auxSpine && !pcActiveBody->hasObject(auxSpine)
             && !pcActiveBody->getOrigin()->hasObject(auxSpine)) {
        extReference = true;
    }
    else {
        for (App::DocumentObject* obj : pipe->Sections.getValues()) {
            if (!pcActiveBody->hasObject(obj) && !pcActiveBody->getOrigin()->hasObject(obj)) {
                extReference = true;
                break;
            }
        }
    }

    if (extReference) {
        QDialog dia(Gui::getMainWindow());
        Ui_DlgReference dlg;
        dlg.setupUi(&dia);
        dia.setModal(true);
        int result = dia.exec();
        if (result == QDialog::DialogCode::Rejected) {
            return false;
        }

        if (!dlg.radioXRef->isChecked()) {
            if (!pcActiveBody->hasObject(spine) && !pcActiveBody->getOrigin()->hasObject(spine)) {
                pipe->Spine.setValue(
                    PartDesignGui::TaskFeaturePick::makeCopy(spine,
                                                             "",
                                                             dlg.radioIndependent->isChecked()),
                    pipe->Spine.getSubValues());
                copies.push_back(pipe->Spine.getValue());
            }
            else if (!pcActiveBody->hasObject(auxSpine)
                     && !pcActiveBody->getOrigin()->hasObject(auxSpine)) {
                pipe->AuxiliarySpine.setValue(
                    PartDesignGui::TaskFeaturePick::makeCopy(auxSpine,
                                                             "",
                                                             dlg.radioIndependent->isChecked()),
                    pipe->AuxiliarySpine.getSubValues());
                copies.push_back(pipe->AuxiliarySpine.getValue());
            }

            std::vector<App::PropertyLinkSubList::SubSet> subSets;
            for (auto& subSet : pipe->Sections.getSubListValues()) {
                if (!pcActiveBody->hasObject(subSet.first)
                    && !pcActiveBody->getOrigin()->hasObject(subSet.first)) {
                    subSets.emplace_back(
                        PartDesignGui::TaskFeaturePick::makeCopy(subSet.first,
                                                                 "",
                                                                 dlg.radioIndependent->isChecked()),
                        subSet.second);
                    copies.push_back(subSets.back().first);
                }
                else {
                    subSets.push_back(subSet);
                }
            }

            pipe->Sections.setSubListValues(subSets);
        }
    }

    try {
        setVisibilityOfSpineAndProfile();

        App::DocumentObject* spine = pipe->Spine.getValue();
        std::vector<std::string> subNames = pipe->Spine.getSubValues();
        App::PropertyLinkT propT(spine, subNames);
        Gui::cmdAppObjectArgs(pipe, "Spine = %s", propT.getPropertyPython());

        Gui::cmdAppDocument(pipe, "recompute()");
        if (!getObject()->isValid()) {
            throw Base::RuntimeError(getObject()->getStatusString());
        }
        Gui::cmdGuiDocument(pipe, "resetEdit()");
        Gui::Command::commitCommand();

        // we need to add the copied features to the body after the command action, as otherwise
        // FreeCAD crashes unexplainably
        for (auto obj : copies) {
            pcActiveBody->addObject(obj);
        }
    }
    catch (const Base::Exception& e) {
        QMessageBox::warning(this,
                             tr("Input error"),
                             QApplication::translate("Exception", e.what()));
        return false;
    }

    return true;
}


//**************************************************************************
//**************************************************************************
// Task Orientation
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskPipeOrientation::TaskPipeOrientation(ViewProviderPipe* PipeView,
                                         bool /*newObj*/,
                                         QWidget* parent)
    : TaskSketchBasedParameters(PipeView,
                                parent,
                                "PartDesign_AdditivePipe",
                                tr("Section orientation"))
    , ui(new Ui_TaskPipeOrientation)
    , stateHandler(nullptr)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    // clang-format off
    // some buttons are handled in a buttongroup
    connect(ui->comboBoxMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TaskPipeOrientation::onOrientationChanged);
    connect(ui->buttonProfileClear, &QToolButton::clicked,
            this, &TaskPipeOrientation::onClearButton);
    connect(ui->stackedWidget, &QStackedWidget::currentChanged,
            this, &TaskPipeOrientation::updateUI);
    connect(ui->curvilinear, &QCheckBox::toggled,
            this, &TaskPipeOrientation::onCurvilinearChanged);
    connect(ui->doubleSpinBoxX, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TaskPipeOrientation::onBinormalChanged);
    connect(ui->doubleSpinBoxY, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TaskPipeOrientation::onBinormalChanged);
    connect(ui->doubleSpinBoxZ, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &TaskPipeOrientation::onBinormalChanged);
    // clang-format on

    // Create context menu
    QAction* remove = new QAction(tr("Remove"), this);
    remove->setShortcut(Gui::QtTools::deleteKeySequence());
    remove->setShortcutContext(Qt::WidgetShortcut);

    // display shortcut behind the context menu entry
    remove->setShortcutVisibleInContextMenu(true);

    ui->listWidgetReferences->addAction(remove);
    connect(remove, &QAction::triggered, this, &TaskPipeOrientation::onDeleteItem);
    ui->listWidgetReferences->setContextMenuPolicy(Qt::ActionsContextMenu);

    this->groupLayout()->addWidget(proxy);

    PartDesign::Pipe* pipe = PipeView->getObject<PartDesign::Pipe>();

    // add initial values
    if (pipe->AuxiliarySpine.getValue()) {
        ui->profileBaseEdit->setText(
            QString::fromUtf8(pipe->AuxiliarySpine.getValue()->Label.getValue()));
    }

    std::vector<std::string> strings = pipe->AuxiliarySpine.getSubValues();
    for (const auto& string : strings) {
        QString label = QString::fromStdString(string);
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(label);
        item->setData(Qt::UserRole, QByteArray(label.toUtf8()));
        ui->listWidgetReferences->addItem(item);
    }

    ui->comboBoxMode->setCurrentIndex(pipe->Mode.getValue());
    ui->curvilinear->setChecked(pipe->AuxiliaryCurvilinear.getValue());

    // should be called after panel has become visible
    QMetaObject::invokeMethod(this,
                              "updateUI",
                              Qt::QueuedConnection,
                              Q_ARG(int, pipe->Mode.getValue()));
    this->blockSelection(false);
}

TaskPipeOrientation::~TaskPipeOrientation()
{
    try {
        if (auto view = getViewObject<ViewProviderPipe>()) {
            view->highlightReferences(ViewProviderPipe::AuxiliarySpine, false);
        }
    }
    catch (const Standard_OutOfRange&) {
    }
}

void TaskPipeOrientation::onOrientationChanged(int idx)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        pipe->Mode.setValue(idx);
        recomputeFeature();
    }
}

void TaskPipeOrientation::clearButtons()
{
    ui->buttonRefAdd->setChecked(false);
    ui->buttonRefRemove->setChecked(false);
    ui->buttonProfileBase->setChecked(false);
}

void TaskPipeOrientation::exitSelectionMode()
{
    Gui::Selection().clearSelection();
}

void TaskPipeOrientation::onClearButton()
{
    ui->listWidgetReferences->clear();
    ui->profileBaseEdit->clear();
    if (auto view = getViewObject<ViewProviderPipe>()) {
        view->highlightReferences(ViewProviderPipe::AuxiliarySpine, false);
        getObject<PartDesign::Pipe>()->AuxiliarySpine.setValue(nullptr);
    }
}

void TaskPipeOrientation::onCurvilinearChanged(bool checked)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        pipe->AuxiliaryCurvilinear.setValue(checked);
        recomputeFeature();
    }
}

void TaskPipeOrientation::onBinormalChanged(double)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        Base::Vector3d vec(ui->doubleSpinBoxX->value(),
                           ui->doubleSpinBoxY->value(),
                           ui->doubleSpinBoxZ->value());

        pipe->Binormal.setValue(vec);
        recomputeFeature();
    }
}

void TaskPipeOrientation::onSelectionChanged(const SelectionChanges& msg)
{
    if (stateHandler->getSelectionMode() == StateHandlerTaskPipe::SelectionModes::none) {
        return;
    }

    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        if (referenceSelected(msg)) {
            if (stateHandler->getSelectionMode()
                == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeAdd) {
                QString sub = QString::fromStdString(msg.pSubName);
                if (!sub.isEmpty()) {
                    QListWidgetItem* item = new QListWidgetItem();
                    item->setText(sub);
                    item->setData(Qt::UserRole, QByteArray(msg.pSubName));
                    ui->listWidgetReferences->addItem(item);
                }

                App::Document* document = App::GetApplication().getDocument(msg.pDocName);
                App::DocumentObject* object =
                    document ? document->getObject(msg.pObjectName) : nullptr;
                if (object) {
                    QString label = QString::fromUtf8(object->Label.getValue());
                    ui->profileBaseEdit->setText(label);
                }
            }
            else if (stateHandler->getSelectionMode()
                     == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeRemove) {
                QString sub = QString::fromLatin1(msg.pSubName);
                if (!sub.isEmpty()) {
                    removeFromListWidget(ui->listWidgetReferences, sub);
                }
                else {
                    ui->profileBaseEdit->clear();
                }
            }
            else if (stateHandler->getSelectionMode()
                     == StateHandlerTaskPipe::SelectionModes::refAuxSpine) {
                ui->listWidgetReferences->clear();

                App::Document* document = App::GetApplication().getDocument(msg.pDocName);
                App::DocumentObject* object =
                    document ? document->getObject(msg.pObjectName) : nullptr;
                if (object) {
                    QString label = QString::fromUtf8(object->Label.getValue());
                    ui->profileBaseEdit->setText(label);
                }
            }

            clearButtons();
            auto view = getViewObject<ViewProviderPipe>();
            view->highlightReferences(ViewProviderPipe::AuxiliarySpine, false);
            recomputeFeature();
        }

        clearButtons();
        exitSelectionMode();
    }
}

bool TaskPipeOrientation::referenceSelected(const SelectionChanges& msg) const
{
    auto selectionMode = stateHandler->getSelectionMode();

    if (msg.Type == Gui::SelectionChanges::AddSelection
        && (selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpine
            || selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeAdd
            || selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeRemove)) {
        if (strcmp(msg.pDocName, getObject()->getDocument()->getName()) != 0) {
            return false;
        }

        // not allowed to reference ourself
        const char* fname = getObject()->getNameInDocument();
        if (strcmp(msg.pObjectName, fname) == 0) {
            return false;
        }

        if (const auto pipe = getObject<PartDesign::Pipe>()) {
            // change the references
            const std::string subName(msg.pSubName);
            std::vector<std::string> refs = pipe->AuxiliarySpine.getSubValues();
            const auto f = std::ranges::find(refs, subName);

            if (selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpine) {
                refs.clear();
            }
            else if (selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeAdd) {
                if (f != refs.end()) {
                    return false;  // duplicate selection
                }

                refs.push_back(subName);
            }
            else if (selectionMode == StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeRemove) {
                if (f == refs.end()) {
                    return false;
                }

                refs.erase(f);
            }

            App::Document* doc = pipe->getDocument();
            pipe->AuxiliarySpine.setValue(doc->getObject(msg.pObjectName), refs);
            return true;
        }
    }

    return false;
}

void TaskPipeOrientation::removeFromListWidget(QListWidget* widget, QString name)
{
    QList<QListWidgetItem*> items = widget->findItems(name, Qt::MatchExactly);
    if (!items.empty()) {
        for (auto item : items) {
            QListWidgetItem* it = widget->takeItem(widget->row(item));
            delete it;
        }
    }
}

void TaskPipeOrientation::onDeleteItem()
{
    // Delete the selected spine
    int row = ui->listWidgetReferences->currentRow();
    QListWidgetItem* item = ui->listWidgetReferences->takeItem(row);
    if (item) {
        QByteArray data = item->data(Qt::UserRole).toByteArray();
        delete item;

        // search inside the list of spines
        if (const auto pipe = getObject<PartDesign::Pipe>()) {
            std::vector<std::string> refs = pipe->AuxiliarySpine.getSubValues();
            const std::string obj = data.constData();

            // if something was found, delete it and update the spine list
            if (const auto f = std::ranges::find(refs, obj); f != refs.end()) {
                refs.erase(f);
                pipe->AuxiliarySpine.setValue(pipe->AuxiliarySpine.getValue(), refs);
                clearButtons();
                recomputeFeature();
            }
        }
    }
}

void TaskPipeOrientation::updateUI(int idx)
{
    // make sure we resize to the size of the current page
    for (int i = 0; i < ui->stackedWidget->count(); ++i) {
        ui->stackedWidget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    if (idx < ui->stackedWidget->count()) {
        ui->stackedWidget->widget(idx)->setSizePolicy(QSizePolicy::Expanding,
                                                      QSizePolicy::Expanding);
    }
}


//**************************************************************************
//**************************************************************************
// Task Scaling
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
TaskPipeScaling::TaskPipeScaling(ViewProviderPipe* PipeView, bool /*newObj*/, QWidget* parent)
    : TaskSketchBasedParameters(PipeView,
                                parent,
                                "PartDesign_AdditivePipe",
                                tr("Section transformation"))
    , ui(new Ui_TaskPipeScaling)
    , stateHandler(nullptr)
{
    // we need a separate container widget to add all controls to
    proxy = new QWidget(this);
    ui->setupUi(proxy);
    QMetaObject::connectSlotsByName(this);

    // some buttons are handled in a buttongroup
    connect(ui->comboBoxScaling,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPipeScaling::onScalingChanged);
    connect(ui->stackedWidget, &QStackedWidget::currentChanged, this, &TaskPipeScaling::updateUI);

    // Create context menu
    QAction* remove = new QAction(tr("Remove"), this);
    remove->setShortcut(Gui::QtTools::deleteKeySequence());
    remove->setShortcutContext(Qt::WidgetShortcut);

    // display shortcut behind the context menu entry
    remove->setShortcutVisibleInContextMenu(true);

    ui->listWidgetReferences->addAction(remove);
    ui->listWidgetReferences->setContextMenuPolicy(Qt::ActionsContextMenu);
    connect(remove, &QAction::triggered, this, &TaskPipeScaling::onDeleteSection);

    connect(ui->listWidgetReferences->model(),
            &QAbstractListModel::rowsMoved,
            this,
            &TaskPipeScaling::indexesMoved);

    this->groupLayout()->addWidget(proxy);

    PartDesign::Pipe* pipe = PipeView->getObject<PartDesign::Pipe>();
    for (auto& subSet : pipe->Sections.getSubListValues()) {
        Gui::Application::Instance->showViewProvider(subSet.first);
        QString label = make2DLabel(subSet.first, subSet.second);
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(label);
        item->setData(Qt::UserRole, QVariant::fromValue(subSet));
        ui->listWidgetReferences->addItem(item);
    }

    ui->comboBoxScaling->setCurrentIndex(pipe->Transformation.getValue());

    // should be called after panel has become visible
    QMetaObject::invokeMethod(this,
                              "updateUI",
                              Qt::QueuedConnection,
                              Q_ARG(int, pipe->Transformation.getValue()));
    this->blockSelection(false);
}

TaskPipeScaling::~TaskPipeScaling()
{
    try {
        if (auto view = getViewObject<ViewProviderPipe>()) {
            view->highlightReferences(ViewProviderPipe::Section, false);
        }
    }
    catch (const Standard_OutOfRange&) {
    }
}

void TaskPipeScaling::indexesMoved()
{
    QAbstractItemModel* model = qobject_cast<QAbstractItemModel*>(sender());
    if (!model) {
        return;
    }

    if (auto pipe = getObject<PartDesign::Pipe>()) {
        auto originals = pipe->Sections.getSubListValues();
        int rows = model->rowCount();
        for (int i = 0; i < rows; i++) {
            QModelIndex index = model->index(i, 0);
            originals[i] = index.data(Qt::UserRole).value<App::PropertyLinkSubList::SubSet>();
        }

        pipe->Sections.setSubListValues(originals);
        recomputeFeature();
        updateUI(ui->stackedWidget->currentIndex());
    }
}

void TaskPipeScaling::clearButtons()
{
    ui->buttonRefRemove->setChecked(false);
    ui->buttonRefAdd->setChecked(false);
}

void TaskPipeScaling::exitSelectionMode()
{
    Gui::Selection().clearSelection();
}

void TaskPipeScaling::onScalingChanged(int idx)
{
    if (auto pipe = getObject<PartDesign::Pipe>()) {
        updateUI(idx);
        pipe->Transformation.setValue(idx);
    }
}

void TaskPipeScaling::onSelectionChanged(const SelectionChanges& msg)
{
    if (stateHandler->getSelectionMode() == StateHandlerTaskPipe::SelectionModes::none) {
        return;
    }

    if (msg.Type == Gui::SelectionChanges::AddSelection) {
        if (referenceSelected(msg)) {
            App::Document* document = App::GetApplication().getDocument(msg.pDocName);
            App::DocumentObject* object = document ? document->getObject(msg.pObjectName) : nullptr;
            if (object) {
                QString label = make2DLabel(object, {msg.pSubName});
                if (stateHandler->getSelectionMode()
                    == StateHandlerTaskPipe::SelectionModes::refSectionAdd) {
                    QListWidgetItem* item = new QListWidgetItem();
                    item->setText(label);
                    item->setData(
                        Qt::UserRole,
                        QVariant::fromValue(
                            std::make_pair(object, std::vector<std::string>(1, msg.pSubName))));
                    ui->listWidgetReferences->addItem(item);
                }
                else if (stateHandler->getSelectionMode()
                         == StateHandlerTaskPipe::SelectionModes::refSectionRemove) {
                    removeFromListWidget(ui->listWidgetReferences, label);
                }
            }

            clearButtons();
            recomputeFeature();
        }
        clearButtons();
        exitSelectionMode();
    }
}

bool TaskPipeScaling::referenceSelected(const SelectionChanges& msg) const
{
    auto selectionMode = stateHandler->getSelectionMode();

    if ((msg.Type == Gui::SelectionChanges::AddSelection)
        && ((selectionMode == StateHandlerTaskPipe::SelectionModes::refSectionAdd)
            || (selectionMode == StateHandlerTaskPipe::SelectionModes::refSectionRemove))) {
        if (strcmp(msg.pDocName, getObject()->getDocument()->getName()) != 0) {
            return false;
        }

        // not allowed to reference ourself
        const char* fname = getObject()->getNameInDocument();
        if (strcmp(msg.pObjectName, fname) == 0) {
            return false;
        }

        // change the references
        if (const auto pipe = getObject<PartDesign::Pipe>()) {
            std::vector<App::DocumentObject*> refs = pipe->Sections.getValues();
            App::DocumentObject* obj = pipe->getDocument()->getObject(msg.pObjectName);
            const auto f = std::ranges::find(refs, obj);

            if (selectionMode == StateHandlerTaskPipe::SelectionModes::refSectionAdd) {
                if (f != refs.end()) {
                    return false;  // duplicate selection
                }

                pipe->Sections.addValue(obj, {msg.pSubName});
            }
            else {
                if (f == refs.end()) {
                    return false;
                }

                pipe->Sections.removeValue(obj);
            }

            auto view = getViewObject<ViewProviderPipe>();
            view->highlightReferences(ViewProviderPipe::Section, false);
            return true;
        }
    }

    return false;
}

void TaskPipeScaling::removeFromListWidget(QListWidget* widget, QString name)
{
    QList<QListWidgetItem*> items = widget->findItems(name, Qt::MatchExactly);
    if (!items.empty()) {
        for (auto item : items) {
            QListWidgetItem* it = widget->takeItem(widget->row(item));
            delete it;
        }
    }
}

void TaskPipeScaling::onDeleteSection()
{
    // Delete the selected profile
    int row = ui->listWidgetReferences->currentRow();
    QListWidgetItem* item = ui->listWidgetReferences->takeItem(row);
    if (item) {
        QByteArray data(item->data(Qt::UserRole)
                            .value<App::PropertyLinkSubList::SubSet>()
                            .first->getNameInDocument());
        delete item;

        if (const auto pipe = getObject<PartDesign::Pipe>()) {
            std::vector<App::DocumentObject*> refs = pipe->Sections.getValues();
            App::DocumentObject* obj = pipe->getDocument()->getObject(data.constData());

            if (const auto f = std::ranges::find(refs.begin(), refs.end(), obj); f != refs.end()) {
                pipe->Sections.removeValue(obj);
                clearButtons();
                recomputeFeature();
            }
        }
    }
}

void TaskPipeScaling::updateUI(int idx)
{
    // make sure we resize to the size of the current page
    for (int i = 0; i < ui->stackedWidget->count(); ++i) {
        ui->stackedWidget->widget(i)->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    }

    if (idx < ui->stackedWidget->count()) {
        ui->stackedWidget->widget(idx)->setSizePolicy(QSizePolicy::Expanding,
                                                      QSizePolicy::Expanding);
    }
}


//**************************************************************************
//**************************************************************************
// TaskDialog
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskDlgPipeParameters::TaskDlgPipeParameters(ViewProviderPipe* PipeView, bool newObj)
    : TaskDlgSketchBasedParameters(PipeView)
{
    assert(PipeView);
    parameter = new TaskPipeParameters(PipeView, newObj);
    orientation = new TaskPipeOrientation(PipeView, newObj);
    scaling = new TaskPipeScaling(PipeView, newObj);

    stateHandler = new StateHandlerTaskPipe();

    Content.push_back(parameter);
    Content.push_back(orientation);
    Content.push_back(scaling);

    parameter->stateHandler = stateHandler;
    orientation->stateHandler = stateHandler;
    scaling->stateHandler = stateHandler;

    buttonGroup = new ButtonGroup(this);
    buttonGroup->setExclusive(true);

    buttonGroup->addButton(parameter->ui->buttonProfileBase, StateHandlerTaskPipe::refProfile);
    buttonGroup->addButton(parameter->ui->buttonSpineBase, StateHandlerTaskPipe::refSpine);
    buttonGroup->addButton(parameter->ui->buttonRefAdd, StateHandlerTaskPipe::refSpineEdgeAdd);
    buttonGroup->addButton(parameter->ui->buttonRefRemove,
                           StateHandlerTaskPipe::refSpineEdgeRemove);

    buttonGroup->addButton(orientation->ui->buttonProfileBase, StateHandlerTaskPipe::refAuxSpine);
    buttonGroup->addButton(orientation->ui->buttonRefAdd, StateHandlerTaskPipe::refAuxSpineEdgeAdd);
    buttonGroup->addButton(orientation->ui->buttonRefRemove,
                           StateHandlerTaskPipe::refAuxSpineEdgeRemove);

    buttonGroup->addButton(scaling->ui->buttonRefAdd, StateHandlerTaskPipe::refSectionAdd);
    buttonGroup->addButton(scaling->ui->buttonRefRemove, StateHandlerTaskPipe::refSectionRemove);

    connect(buttonGroup,
            qOverload<QAbstractButton*, bool>(&QButtonGroup::buttonToggled),
            this,
            &TaskDlgPipeParameters::onButtonToggled);
}

TaskDlgPipeParameters::~TaskDlgPipeParameters()
{
    delete stateHandler;
}

void TaskDlgPipeParameters::onButtonToggled(QAbstractButton* button, bool checked)
{
    int id = buttonGroup->id(button);

    if (checked) {
        // hideObject();
        Gui::Selection().clearSelection();
        stateHandler->selectionMode = static_cast<StateHandlerTaskPipe::SelectionModes>(id);
    }
    else {
        Gui::Selection().clearSelection();
        if (stateHandler->selectionMode == static_cast<StateHandlerTaskPipe::SelectionModes>(id)) {
            stateHandler->selectionMode = StateHandlerTaskPipe::SelectionModes::none;
        }
    }

    switch (id) {
        case StateHandlerTaskPipe::SelectionModes::refProfile:
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Profile,
                                                                   checked);
            break;
        case StateHandlerTaskPipe::SelectionModes::refSpine:
        case StateHandlerTaskPipe::SelectionModes::refSpineEdgeAdd:
        case StateHandlerTaskPipe::SelectionModes::refSpineEdgeRemove:
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Spine,
                                                                   checked);
            break;
        case StateHandlerTaskPipe::SelectionModes::refAuxSpine:
        case StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeAdd:
        case StateHandlerTaskPipe::SelectionModes::refAuxSpineEdgeRemove:
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::AuxiliarySpine,
                                                                   checked);
            break;
        case StateHandlerTaskPipe::SelectionModes::refSectionAdd:
        case StateHandlerTaskPipe::SelectionModes::refSectionRemove:
            getViewObject<ViewProviderPipe>()->highlightReferences(ViewProviderPipe::Section,
                                                                   checked);
            break;
        default:
            break;
    }
}

//==== calls from the TaskView ===============================================================


bool TaskDlgPipeParameters::accept()
{
    return parameter->accept();
}


#include "moc_TaskPipeParameters.cpp"
