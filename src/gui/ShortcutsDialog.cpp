/*
 * Stellarium
 * Copyright (C) 2012 Anton Samoylov
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#include <QDialog>
#include <QStandardItemModel>

#include "StelApp.hpp"
#include "StelShortcutMgr.hpp"
#include "StelTranslator.hpp"
#include "StelShortcutGroup.hpp"

#include "ShortcutLineEdit.hpp"
#include "ShortcutsDialog.hpp"
#include "ui_shortcutsDialog.h"




ShortcutsFilterModel::ShortcutsFilterModel(QObject* parent) :
    QSortFilterProxyModel(parent)
{
	//
}

bool ShortcutsFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	if (filterRegExp().isEmpty())
		return true;
	
	if (source_parent.isValid())
	{
		QModelIndex index = source_parent.child(source_row, filterKeyColumn());
		QString data = sourceModel()->data(index, filterRole()).toString();
		return data.contains(filterRegExp());
	}
	else
	{
		QModelIndex index = sourceModel()->index(source_row, filterKeyColumn());
		for (int row = 0; row < sourceModel()->rowCount(index); row++)
		{
			if (filterAcceptsRow(row, index))
				return true;
		}
	}
	return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}


ShortcutsDialog::ShortcutsDialog() :
	ui(new Ui_shortcutsDialogForm),
	filterModel(new ShortcutsFilterModel(this)),
	mainModel(new QStandardItemModel(this))
{
	shortcutMgr = StelApp::getInstance().getStelShortcutManager();
}

ShortcutsDialog::~ShortcutsDialog()
{
	collisionItems.clear();
	delete ui;
	ui = NULL;
}

void ShortcutsDialog::drawCollisions()
{
	QBrush brush(Qt::red);
	foreach(QStandardItem* item, collisionItems)
	{
		// change colors of all columns for better visibility
		item->setForeground(brush);
		QModelIndex index = item->index();
		mainModel->itemFromIndex(index.sibling(index.row(), 1))->setForeground(brush);
		mainModel->itemFromIndex(index.sibling(index.row(), 2))->setForeground(brush);
	}
}

void ShortcutsDialog::resetCollisions()
{
	QBrush brush =
	        ui->shortcutsTreeView->palette().brush(QPalette::Foreground);
	foreach(QStandardItem* item, collisionItems)
	{
		item->setForeground(brush);
		QModelIndex index = item->index();
		mainModel->itemFromIndex(index.sibling(index.row(), 1))->setForeground(brush);
		mainModel->itemFromIndex(index.sibling(index.row(), 2))->setForeground(brush);
	}
	collisionItems.clear();
}

void ShortcutsDialog::retranslate()
{
	if (dialog)
	{
		ui->retranslateUi(dialog);
		setModelHeader();
		updateTreeData();
	}
}

void ShortcutsDialog::initEditors()
{
	QModelIndex index = filterModel->mapToSource(ui->shortcutsTreeView->currentIndex());
	index = index.sibling(index.row(), 0);
	QStandardItem* currentItem = mainModel->itemFromIndex(index);
	if (itemIsEditable(currentItem))
	{
		// current item is shortcut, not group (group items aren't selectable)
		ui->primaryShortcutEdit->setEnabled(true);
		ui->altShortcutEdit->setEnabled(true);
		// fill editors with item's shortcuts
		QVariant data = mainModel->data(index.sibling(index.row(), 1));
		ui->primaryShortcutEdit->setContents(data.value<QKeySequence>());
		data = mainModel->data(index.sibling(index.row(), 2));
		ui->altShortcutEdit->setContents(data.value<QKeySequence>());
	}
	else
	{
		// item is group, not shortcut
		ui->primaryShortcutEdit->setEnabled(false);
		ui->altShortcutEdit->setEnabled(false);
		ui->applyButton->setEnabled(false);
		ui->primaryShortcutEdit->clear();
		ui->altShortcutEdit->clear();
	}
	polish();
}

bool ShortcutsDialog::prefixMatchKeySequence(const QKeySequence& ks1,
                                             const QKeySequence& ks2)
{
	if (ks1.isEmpty() || ks2.isEmpty())
	{
		return false;
	}
	for (uint i = 0; i < qMin(ks1.count(), ks2.count()); ++i)
	{
		if (ks1[i] != ks2[i])
		{
			return false;
		}
	}
	return true;
}

QList<QStandardItem*> ShortcutsDialog::findCollidingItems(QKeySequence ks)
{
	QList<QStandardItem*> result;
	for (int row = 0; row < mainModel->rowCount(); row++)
	{
		QStandardItem* group = mainModel->item(row, 0);
		if (!group->hasChildren())
			continue;
		for (int subrow = 0; subrow < group->rowCount(); subrow++)
		{
			QKeySequence primary(group->child(subrow, 1)
			                     ->data(Qt::DisplayRole).toString());
			QKeySequence secondary(group->child(subrow, 2)
			                       ->data(Qt::DisplayRole).toString());
			if (prefixMatchKeySequence(ks, primary) ||
			    prefixMatchKeySequence(ks, secondary))
				result.append(group->child(subrow, 0));
		}
	}
	return result;
}

void ShortcutsDialog::handleCollisions(ShortcutLineEdit *currentEdit)
{
	resetCollisions();
	
	// handle collisions
	QString text = currentEdit->text();
	collisionItems = findCollidingItems(QKeySequence(text));
	QModelIndex index =
	        filterModel->mapToSource(ui->shortcutsTreeView->currentIndex());
	index = index.sibling(index.row(), 0);
	QStandardItem* currentItem = mainModel->itemFromIndex(index);
	collisionItems.removeOne(currentItem);
	if (!collisionItems.isEmpty())
	{
		drawCollisions();
		ui->applyButton->setEnabled(false);
		// scrolling to first collision item
		QModelIndex first =
		        filterModel->mapFromSource(collisionItems.first()->index());
		ui->shortcutsTreeView->scrollTo(first);
		currentEdit->setProperty("collision", true);
	}
	else
	{
		// scrolling back to current item
		QModelIndex current = filterModel->mapFromSource(index);
		ui->shortcutsTreeView->scrollTo(current);
		currentEdit->setProperty("collision", false);
	}
}

void ShortcutsDialog::handleChanges()
{
	// work only with changed editor
	ShortcutLineEdit* editor = qobject_cast<ShortcutLineEdit*>(sender());
	bool isPrimary = (editor == ui->primaryShortcutEdit);
	// updating clear buttons
	if (isPrimary)
	{
		ui->primaryBackspaceButton->setEnabled(!editor->isEmpty());
	}
	else
	{
		ui->altBackspaceButton->setEnabled(!editor->isEmpty());
	}
	// updating apply button
	QModelIndex index =
	        filterModel->mapToSource(ui->shortcutsTreeView->currentIndex());
	if (!index.isValid() ||
	    (isPrimary &&
	     editor->text() == mainModel->data(index.sibling(index.row(), 1))) ||
	    (!isPrimary &&
	     editor->text() == mainModel->data(index.sibling(index.row(), 2))))
	{
		// nothing to apply
		ui->applyButton->setEnabled(false);
	}
	else
	{
		ui->applyButton->setEnabled(true);
	}
	handleCollisions(editor);
	polish();
}

void ShortcutsDialog::applyChanges() const
{
	// get ids stored in tree
	QModelIndex index =
	        filterModel->mapToSource(ui->shortcutsTreeView->currentIndex());
	if (!index.isValid())
		return;
	index = index.sibling(index.row(), 0);
	QStandardItem* currentItem = mainModel->itemFromIndex(index);
	QString actionId = currentItem->data(Qt::UserRole).toString();
	QString groupId = currentItem->parent()->data(Qt::UserRole).toString();
	// changing keys in shortcuts
	shortcutMgr->changeActionPrimaryKey(actionId, groupId, ui->primaryShortcutEdit->getKeySequence());
	shortcutMgr->changeActionAltKey(actionId, groupId, ui->altShortcutEdit->getKeySequence());
	// no need to change displaying information, as it changed in mgr, and will be updated in connected slot

	// save shortcuts to file
	shortcutMgr->saveShortcuts();

	// nothing to apply until edits' content changes
	ui->applyButton->setEnabled(false);
}

void ShortcutsDialog::switchToEditors(const QModelIndex& index)
{
	QModelIndex mainIndex = filterModel->mapToSource(index);
	QStandardItem* item = mainModel->itemFromIndex(mainIndex);
	if (itemIsEditable(item))
	{
		ui->primaryShortcutEdit->setFocus();
	}
}

void ShortcutsDialog::createDialogContent()
{
	ui->setupUi(dialog);
	
	resetModel();
	filterModel->setSourceModel(mainModel);
	filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	filterModel->setDynamicSortFilter(true);
	filterModel->setSortLocaleAware(true);
	ui->shortcutsTreeView->setModel(filterModel);
	ui->shortcutsTreeView->header()->setMovable(false);
	ui->shortcutsTreeView->sortByColumn(0, Qt::AscendingOrder);
	
	connect(&StelApp::getInstance(), SIGNAL(languageChanged()), this, SLOT(retranslate()));
	connect(ui->shortcutsTreeView->selectionModel(),
	        SIGNAL(currentChanged(QModelIndex,QModelIndex)),
	        this,
	        SLOT(initEditors()));
	connect(ui->shortcutsTreeView,
	        SIGNAL(activated(QModelIndex)),
	        this,
	        SLOT(switchToEditors(QModelIndex)));
	connect(ui->lineEditSearch, SIGNAL(textChanged(QString)),
	        filterModel, SLOT(setFilterFixedString(QString)));
	
	// apply button logic
	connect(ui->applyButton, SIGNAL(clicked()), this, SLOT(applyChanges()));
	// restore defaults button logic
	connect(ui->restoreDefaultsButton, SIGNAL(clicked()), this, SLOT(restoreDefaultShortcuts()));
	// we need to disable all shortcut actions, so we can enter shortcuts without activating any actions
	connect(ui->primaryShortcutEdit, SIGNAL(focusChanged(bool)), shortcutMgr, SLOT(setAllActionsEnabled(bool)));
	connect(ui->altShortcutEdit, SIGNAL(focusChanged(bool)), shortcutMgr, SLOT(setAllActionsEnabled(bool)));
	// handling changes in editors
	connect(ui->primaryShortcutEdit, SIGNAL(contentsChanged()), this, SLOT(handleChanges()));
	connect(ui->altShortcutEdit, SIGNAL(contentsChanged()), this, SLOT(handleChanges()));
	// handle outer shortcuts changes
	connect(shortcutMgr, SIGNAL(shortcutChanged(StelShortcut*)), this, SLOT(updateShortcutsItem(StelShortcut*)));
	
	QString backspaceChar;
	backspaceChar.append(QChar(0x232B)); // Erase left
	//test.append(QChar(0x2672));
	//test.append(QChar(0x267B));
	//test.append(QChar(0x267C));
	//test.append(QChar(0x21BA)); // Counter-clockwise
	//test.append(QChar(0x2221)); // Angle sign
	ui->primaryBackspaceButton->setText(backspaceChar);
	ui->altBackspaceButton->setText(backspaceChar);

	updateTreeData();
}

void ShortcutsDialog::updateText()
{
}

void ShortcutsDialog::polish()
{
	ui->primaryShortcutEdit->style()->unpolish(ui->primaryShortcutEdit);
	ui->primaryShortcutEdit->style()->polish(ui->primaryShortcutEdit);
	ui->altShortcutEdit->style()->unpolish(ui->altShortcutEdit);
	ui->altShortcutEdit->style()->polish(ui->altShortcutEdit);
}

QStandardItem* ShortcutsDialog::updateGroup(StelShortcutGroup* group)
{
	QString groupId = group->getId();
	QStandardItem* groupItem = findItemByData(QVariant(groupId),
	                                          Qt::UserRole);
	bool isNew = false;
	if (!groupItem)
	{
		// create new
		groupItem = new QStandardItem();
		isNew = true;
	}
	// group items aren't selectable, so reset default flag
	groupItem->setFlags(Qt::ItemIsEnabled);
	
	// setup displayed text
	QString groupText = group->getText();
	QString text(q_(groupText.isEmpty() ? groupId : groupText));
	groupItem->setText(text);
	// store id
	groupItem->setData(groupId, Qt::UserRole);
	groupItem->setColumnCount(3);
	// setup bold font for group lines
	QFont rootFont = groupItem->font();
	rootFont.setBold(true);
	rootFont.setPixelSize(14);
	groupItem->setFont(rootFont);
	if (isNew)
		mainModel->appendRow(groupItem);
	
	// expand only enabled group
	bool enabled = group->isEnabled();
	QModelIndex index = filterModel->mapFromSource(groupItem->index());
	if (enabled)
		ui->shortcutsTreeView->expand(index);
	else
		ui->shortcutsTreeView->collapse(index);
	ui->shortcutsTreeView->setFirstColumnSpanned(index.row(),
	                                             QModelIndex(),
	                                             true);
	ui->shortcutsTreeView->setRowHidden(index.row(), QModelIndex(), !enabled);
	
	return groupItem;
}

QStandardItem* ShortcutsDialog::findItemByData(QVariant value, int role, int column)
{
	for (int row = 0; row < mainModel->rowCount(); row++)
	{
		QStandardItem* item = mainModel->item(row, 0);
		if (!item)
			continue; //WTF?
		if (column == 0)
		{
			if (item->data(role) == value)
				return item;
		}
		
		for (int subrow = 0; subrow < item->rowCount(); subrow++)
		{
			QStandardItem* subitem = item->child(subrow, column);
			if (subitem->data(role) == value)
				return subitem;
		}
	}
	return 0;
}

void ShortcutsDialog::updateShortcutsItem(StelShortcut *shortcut,
                                          QStandardItem *shortcutItem)
{
	QVariant shortcutId(shortcut->getId());
	if (shortcutItem == NULL)
	{
		// search for item
		shortcutItem = findItemByData(shortcutId, Qt::UserRole, 0);
	}
	// we didn't find item, create and add new
	QStandardItem* groupItem = NULL;
	if (shortcutItem == NULL)
	{
		// firstly search for group
		QVariant groupId(shortcut->getGroup()->getId());
		groupItem = findItemByData(groupId, Qt::UserRole, 0);
		if (groupItem == NULL)
		{
			// create and add new group to treeWidget
			groupItem = updateGroup(shortcut->getGroup());
		}
		// create shortcut item
		shortcutItem = new QStandardItem();
		shortcutItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		groupItem->appendRow(shortcutItem);
		// store shortcut id, so we can find it when shortcut changed
		shortcutItem->setData(shortcutId, Qt::UserRole);
		QStandardItem* primaryItem = new QStandardItem();
		QStandardItem* secondaryItem = new QStandardItem();
		primaryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		secondaryItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
		groupItem->setChild(shortcutItem->row(), 1, primaryItem);
		groupItem->setChild(shortcutItem->row(), 2, secondaryItem);
	}
	// setup properties of item
	shortcutItem->setText(q_(shortcut->getText()));
	QModelIndex index = shortcutItem->index();
	mainModel->setData(index.sibling(index.row(), 1),
	                   shortcut->getPrimaryKey(), Qt::DisplayRole);
	mainModel->setData(index.sibling(index.row(), 2),
	                   shortcut->getAltKey(), Qt::DisplayRole);
}

void ShortcutsDialog::restoreDefaultShortcuts()
{
	resetModel();
	shortcutMgr->restoreDefaultShortcuts();
	updateTreeData();
	initEditors();
}

void ShortcutsDialog::updateTreeData()
{
	// Create shortcuts tree
	QList<StelShortcutGroup*> groups = shortcutMgr->getGroupList();
	foreach (StelShortcutGroup* group, groups)
	{
		updateGroup(group);
		// display group's shortcuts
		QList<StelShortcut*> shortcuts = group->getActionList();
		foreach (StelShortcut* shortcut, shortcuts)
		{
			updateShortcutsItem(shortcut);
		}
	}
	updateText();
}

bool ShortcutsDialog::itemIsEditable(QStandardItem *item)
{
	if (item == NULL) return false;
	// non-editable items(not group items) have no Qt::ItemIsSelectable flag
	return (Qt::ItemIsSelectable & item->flags());
}

void ShortcutsDialog::resetModel()
{
	mainModel->clear();
	setModelHeader();
}

void ShortcutsDialog::setModelHeader()
{
	// Warning! The latter two strings are reused elsewhere in the GUI.
	QStringList headerLabels;
	headerLabels << q_("Action")
	             << q_("Primary shortcut")
	             << q_("Alternative shortcut");
	mainModel->setHorizontalHeaderLabels(headerLabels);
}