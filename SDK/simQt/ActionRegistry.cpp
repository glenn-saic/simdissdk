/* -*- mode: c++ -*- */
/****************************************************************************
 *****                                                                  *****
 *****                   Classification: UNCLASSIFIED                   *****
 *****                    Classified By:                                *****
 *****                    Declassify On:                                *****
 *****                                                                  *****
 ****************************************************************************
 *
 *
 * Developed by: Naval Research Laboratory, Tactical Electronic Warfare Div.
 *               EW Modeling & Simulation, Code 5773
 *               4555 Overlook Ave.
 *               Washington, D.C. 20375-5339
 *
 * License for source code is in accompanying LICENSE.txt file. If you did
 * not receive a LICENSE.txt with this code, email simdis@nrl.navy.mil.
 *
 * The U.S. Government retains all rights to use, duplicate, distribute,
 * disclose, or release this software.
 *
 */
#include <cassert>
#include <set>
#include <QAction>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include "simQt/QtFormatting.h"
#include "simQt/ActionRegistry.h"

Q_DECLARE_METATYPE(QList<QKeySequence>);

namespace
{
QDataStream &operator<<(QDataStream &out, const QList<QKeySequence> &keys)
{
  out << keys.count();
  for (auto it = keys.begin(); it != keys.end(); ++it)
    out << *it;
  return out;
}

QDataStream &operator>>(QDataStream &in, QList<QKeySequence> &keys)
{
  int numItems = 0;
  in >> numItems;
  for (int k = 0; k < numItems; ++k)
  {
    QKeySequence key;
    in >> key;
    keys.push_back(key);
  }
  return in;
}
}


namespace simQt {

static const char* ORIGINAL_TOOL_TIP_PROPERTY = "OrigTip";

ToolTipUpdater::ToolTipUpdater(QObject* parent)
  : QObject(parent)
{
  timer_ = new QTimer(this);
  timer_->setInterval(0);
  timer_->setSingleShot(true);
  connect(timer_, SIGNAL(timeout()), this, SLOT(updateToolTips_()));
}

void ToolTipUpdater::addPending(simQt::Action* action)
{
  if (action == nullptr)
    return;

  pendingActions_.push_back(action);
  // Start a single shot timer. This allows updating all pending
  // actions at once, once Qt gets control of the event loop.
  timer_->start();
}

void ToolTipUpdater::updateToolTips_()
{
  for (auto it = pendingActions_.begin(); it != pendingActions_.end(); ++it)
  {
    QAction* action = (*it)->action();
    if (!action)
      continue;

    QString tt = action->property(ORIGINAL_TOOL_TIP_PROPERTY).toString(); // Get the original tool tip from the property
    if (tt.isEmpty())
    {
      // No original tool tip. Set it if there's a tool tip, continue otherwise
      const QString currentToolTip = action->toolTip();
      if (currentToolTip.isEmpty())
        continue;

      action->setProperty(ORIGINAL_TOOL_TIP_PROPERTY, currentToolTip);
      tt = currentToolTip;
    }

    QString hkStr = action->shortcut().toString();
    int found = tt.indexOf(HOT_KEY_TAG);
    if (found != -1)
    {
      // This is a tool tip made with a hot key tag telling us where to insert the hot key text
      if (!hkStr.isEmpty())
        hkStr = QString(" (%1)").arg(hkStr); // Format hot key text if hot key exists
      tt.replace(found, HOT_KEY_TAG.size(), hkStr); // Replace the hot key tag with the hot key text (or empty string)
    }
    else if (!hkStr.isEmpty())
    {
      // This is some other kind of tool tip, so just append hot key to the end
      tt.append(QString("\n\nHot Key: %1").arg(hkStr));
    }

    // Set the tool tip
    action->setToolTip(tt);
  }

  pendingActions_.clear();
}

void ToolTipUpdater::removeAction(const simQt::Action* action)
{
  // Action has been removed. Remove from our list of pending actions, if we're tracking it.
  pendingActions_.erase(std::remove(pendingActions_.begin(), pendingActions_.end(), action), pendingActions_.end());
}

////////////////////////////////////////////////

Action::Action(ActionRegistry* registry, const QString& group, const QString& description, QAction* action)
  : registry_(registry),
    group_(group),
    description_(description),
    action_(action)
{
}

Action::~Action()
{
}

QString Action::group() const
{
  return group_;
}

QString Action::description() const
{
  return description_;
}

simQt::ActionRegistry* Action::actionRegistry() const
{
  return registry_;
}

QAction* Action::action() const
{
  return action_;
}

QList<QKeySequence> Action::hotkeys() const
{
  return action_->shortcuts();
}

void Action::execute() const
{
  action_->trigger();
}

int Action::removeHotKey(unsigned int bindingNum)
{
  return registry_->removeHotKey(this, bindingNum);
}

int Action::setHotKey(QKeySequence hotkey)
{
  return registry_->setHotKey(this, hotkey);
}

int Action::setHotKeys(const QList<QKeySequence>& hotkeys)
{
  return registry_->setHotKeys(this, hotkeys);
}

////////////////////////////////////////////////

/** Wide interface implementation for Memento pattern, with ActionRegistry as
 * the originator object.  Includes methods to expand into Serializable Memento pattern
 */
class ActionRegistry::MementoImpl : public ActionRegistry::SettingsMemento
{
public:
  /** multiple keys */
  typedef QList<QKeySequence> HotKeys;

  /** Constructor will save data into a private member for later restoration */
  explicit MementoImpl(const ActionRegistry& registry)
    : data_(MementoImpl::buildFrom_(registry))
  {
  }

  /** Override publicly accessible memento function to restore data */
  virtual int restore(ActionRegistry& registry) const
  {
    return MementoImpl::restoreDestructive_(registry, data_);
  }

  /** Deserializes from a QSettings into an ActionRegistry */
  static int serializeTo(const ActionRegistry& reg, QSettings& settings, const QString& groupName)
  {
    if (settings.status() != QSettings::NoError || !settings.isWritable())
      return 1;
    QMap<QString, HotKeys> keys = MementoImpl::buildFrom_(reg);
    // Use the group namespacing
    settings.beginGroup(groupName);
    settings.remove(""); // Removes all items in current group
    for (QMap<QString, HotKeys>::const_iterator i = keys.begin(); i != keys.end(); ++i)
      settings.setValue(i.key(), QVariant::fromValue<HotKeys>(i.value()));
    settings.endGroup();
    return 0;
  }

  /** Deserializes from a QSettings into an ActionRegistry (note const not possible due to begin/endGroup) */
  static int deserializeFrom(ActionRegistry& reg, QSettings& settings, const QString& groupName)
  {
    if (settings.status() != QSettings::NoError)
      return 1;
    // Build the map of description string to hotkeys
    QMap<QString, HotKeys> data;
    settings.beginGroup(groupName);
    auto allKeys = settings.allKeys();
    for (auto it = allKeys.begin(); it != allKeys.end(); ++it)
      data.insert(*it, settings.value(*it).value<HotKeys>());
    settings.endGroup();
    // Restore it
    return MementoImpl::restoreNonDestructive_(reg, data);
  }

private:

  /** Returns a map of string to list of key sequences for the action */
  static QMap<QString, HotKeys> buildFrom_(const ActionRegistry& reg)
  {
    QMap<QString, HotKeys> rv;
    // Save out the unknowns
    for (QMap<QString, UnknownAction*>::const_iterator i = reg.unknownActions_.begin();
      i != reg.unknownActions_.end(); ++i)
    {
      rv.insert(i.value()->description, i.value()->hotkeys);
    }
    // Save out the known actions
    for (QMap<QString, Action*>::const_iterator i = reg.actionsByDesc_.begin();
      i != reg.actionsByDesc_.end(); ++i)
    {
      const Action* action = i.value();
      // This assertion can fail if an action exists but is in the unknown list.  It
      // indicates the unknown list is in a bad state (because action shouldn't be unknown!)
      assert(!rv.contains(action->description()));
      rv.insert(action->description(), action->hotkeys());
    }
    return rv;
  }

  /** Restores a map of string/hotkey to the registry provided: destructive, removing existing items */
  static int restoreDestructive_(ActionRegistry& registry, const QMap<QString, HotKeys>& keys)
  {
    // Clear out the list of unknowns in the incoming registry, before anything happens.
    for (auto it = registry.unknownActions_.begin(); it != registry.unknownActions_.end(); ++it)
      delete *it;
    registry.unknownActions_.clear();
    registry.unknownActionsByKey_.clear();

    // Restore hotkeys for all actions that are in the registry; iterate by ActionRegistry's
    // list to avoid dereferencing actions that are no longer valid, and to allow us to
    // unset hotkeys that are no longer valid
    QSet<QString> visitedDescs;
    for (auto it = registry.actionsByDesc_.begin(); it != registry.actionsByDesc_.end(); ++it)
    {
      Action* regAction = *it;
      // Set the list of hotkeys; if item is not found it defaults to empty
      HotKeys hotkeys = keys.value(regAction->description(), HotKeys());
      if (hotkeys != regAction->hotkeys())
        registry.setHotKeys(regAction, hotkeys);
      // Save this value for later, so we can find all unknowns in the list
      visitedDescs.insert(regAction->description());
    }

    // Iterate through the remaining items, which will become unknowns.  We set their
    // hotkeys with the addHotKey() public interface
    for (QMap<QString, HotKeys>::const_iterator i = keys.begin(); i != keys.end(); ++i)
    {
      // We only care about items that haven't been visited yet; there are no actions in the registry for these
      if (!visitedDescs.contains(i.key()))
      {
        // Failure of assertion means visitedDesc got constructed improperly
        assert(registry.findWithoutAliases_(i.key()) == nullptr);
        if (i.value().empty())
        {
          // Save empty unknown values so that empty hotkeys can work fine
          UnknownAction* unknown = new UnknownAction;
          unknown->description = i.key();
          registry.unknownActions_[i.key()] = unknown;
        }
        else
        {
          // addHotKey() will take care of unknownActionsByKey_ map
          auto keySeqList = i.value();
          for (auto it = keySeqList.begin(); it != keySeqList.end(); ++it)
            registry.addHotKey(i.key(), *it);
        }
      }
    }

    return 0;
  }

  /** Restores a map of string/hotkey to the registry provided: non-destructive, does not remove existing items */
  static int restoreNonDestructive_(ActionRegistry& registry, const QMap<QString, HotKeys>& keys)
  {
    // Simply override each value for each item in the list
    int rv = 0;
    for (QMap<QString, HotKeys>::const_iterator i = keys.begin(); i != keys.end(); ++i)
    {
      Action* action = registry.findWithoutAliases_(i.key());

      if (action == nullptr)
      {
        // The action does not exist; add a new one (will show up as Unknown)
        if (i.value().empty())
        {
          // Only add if key is unique to unknownActions_
          if (!registry.unknownActions_.contains(i.key()))
          {
            // Store an empty unknown value so that end users can clear out values
            UnknownAction* unknown = new UnknownAction;
            unknown->description = i.key();
            registry.unknownActions_[i.key()] = unknown;
          }
        }
        else
        {
          // addHotKey will handle unknownActionsByKey_
          auto keySeqList = i.value();
          for (auto it = keySeqList.begin(); it != keySeqList.end(); ++it)
            rv += (registry.addHotKey(i.key(), *it) == 0 ? 0 : 1);
        }
      }
      else
      {
        // Acton does exist; reassign its hotkeys
        rv += (registry.setHotKeys(action, i.value()) == 0 ? 0 : 1);
      }
    }

    return rv;
  }

  /** Memento data that maps description to hotkey list */
  QMap<QString, HotKeys> data_;
};

////////////////////////////////////////////////

ActionRegistry::ActionRegistry(QWidget* mainWindow)
  : mainWindow_(mainWindow),
  toolTipUpdater_(new ToolTipUpdater)
{
  connect(this, SIGNAL(hotKeysChanged(simQt::Action*)), toolTipUpdater_, SLOT(addPending(simQt::Action*)));
  connect(this, SIGNAL(actionRemoved(const simQt::Action*)), toolTipUpdater_, SLOT(removeAction(const simQt::Action*)));
}

ActionRegistry::~ActionRegistry()
{
  for (auto it = actionsByDesc_.begin(); it != actionsByDesc_.end(); ++it)
    delete *it;
  for (auto it = unknownActions_.begin(); it != unknownActions_.end(); ++it)
    delete *it;

  delete toolTipUpdater_;
  toolTipUpdater_ = nullptr;
}

Action* ActionRegistry::registerAction(const QString &group, const QString &description, QAction *action)
{
  assertActionsByKeyValid_();

  Action* newAct = findWithoutAliases_(description);
  if (newAct != nullptr)
  {
    // This occurs when more than one action has the same name.  The description must be
    // unique, so this means you have duplicates and need to resolve this issue.
    assert(0);
    return newAct;
  }

  // Note that it's valid to register the same QAction with different group/description.
  // The intent could be to provide multiple bindings or names for the same action, for
  // the purpose of hotkeys or for backwards compatibility with old hotkey names

  // Initialize and set the hotkeys based on original shortcuts and user-provided values
  newAct = new Action(this, group, description, action);
  QList<QKeySequence> originalKeys = action->shortcuts();

  // If the registry knows about the action, clear out the original keys because the end
  // user has already cleared them out before.  This means incoming keys will only default
  // on the first time the action is seen.
  if (unknownActions_.count(description))
    originalKeys.clear();
  QList<QKeySequence> unknownKeys = takeUnknown_(description);
  actionsByDesc_[description] = newAct;
  combineAndSetKeys_(newAct, originalKeys, unknownKeys);

  // Put in main window scope
  if (mainWindow_)
  {
    mainWindow_->addAction(action);
    assert(mainWindow_->actions().count(action) == 1);
  }
  Q_EMIT(actionAdded(newAct));

  // Validate the actions are valid
  assertActionsByKeyValid_();
  return newAct;
}

int ActionRegistry::registerAlias(const QString& actionDesc, const QString& alias)
{
  Action* action = findWithoutAliases_(actionDesc);
  if (action == nullptr)
    return 1;

  if (aliases_.find(alias) != aliases_.end())
    return 1;

  aliases_[alias] = actionDesc;
  return 0;
}

int ActionRegistry::execute(const QString &actionDesc)
{
  Action* action = findAction(actionDesc);

  if (action)
  {
    action->execute();
    return 0;
  }

  return 1;
}

void ActionRegistry::assertActionsByKeyValid_() const
{
#ifndef NDEBUG
  // Make sure that each action in actionsByKey_ has the entry in the list
  auto keys = actionsByKey_.keys();
  for (auto it = keys.begin(); it != keys.end(); ++it)
  {
    const Action* action = actionsByKey_.value(*it);
    assert(action != nullptr && action->hotkeys().contains(*it));
  }

  // Loop through the hotkeys in all known actions and make sure there's an entry and it's us
  auto actions = actionsByDesc_.values();
  for (auto it = actions.begin(); it != actions.end(); ++it)
  {
    const Action* action = *it;
    assert(action != nullptr);
    auto hotkeys = action->hotkeys();
    for (auto hotkeyIt = hotkeys.begin(); hotkeyIt != hotkeys.end(); ++hotkeyIt)
    {
      assert(actionsByKey_.contains(*hotkeyIt));
      assert(actionsByKey_.value(*hotkeyIt) == action);
    }
  }
#endif
}

int ActionRegistry::removeAction(const QString& desc)
{
  assertActionsByKeyValid_();

  Action* action = findWithoutAliases_(desc);
  if (action == nullptr)
    return 1;

  // Remove from actions maps
  actionsByDesc_.remove(desc);

  // Save the bindings in the unknown list.  Note that we cannot rely 100% on the
  // action->hotkeys() value because the end user needs access to the QAction directly
  // and the shortcuts could be changed through the QAction interface.  To prevent
  // stale memory, we have a O(n) search here to remove hot keys.
  QList<QKeySequence> removeKeys;
  for (QMap<QKeySequence, Action*>::iterator i = actionsByKey_.begin(); i != actionsByKey_.end(); ++i)
  {
    if (i.value() == action)
      removeKeys.push_back(i.key());
  }
  // Now remove the keys we know about
  for (auto it = removeKeys.begin(); it != removeKeys.end(); ++it)
  {
    // Remove it from our normal bindings list
    actionsByKey_.remove(*it);
    // Save it to the unknown list
    addHotKey(desc, *it);
  }

  // Make sure the action is not in the actions-by-keys.  Failure means that we have an inconsistency
  // between actionsByKey_ and the action's hotkey list (action gained or lost a hotkey and doesn't
  // map into the actions-by-key).  This means stale memory exists and we'll (maybe) crash later.
  assert(actionsByKey_.values().indexOf(action) == -1);

  // Remove any aliases
  QStringList aliasNames;
  for (QMap<QString, QString>::iterator i = aliases_.begin(); i != aliases_.end(); ++i)
  {
    if (i.value() == desc)
      aliasNames.push_back(i.key());
  }

  for (auto it = aliasNames.begin(); it != aliasNames.end(); ++it)
    aliases_.remove(*it);

  // Remove it from the main window's action list
  if (mainWindow_ && action->action())
    mainWindow_->removeAction(action->action());
  Q_EMIT(actionRemoved(action));

  delete action;

  // Ensure internal consistency at this check point
  assertActionsByKeyValid_();

  return 0;
}

int ActionRegistry::removeUnknownAction(const QString& desc)
{
  auto it = unknownActions_.find(desc);
  if (it == unknownActions_.end())
    return 1;

  for (auto i = (*it)->hotkeys.begin(); i != (*it)->hotkeys.end(); ++i)
    unknownActionsByKey_.remove(*i);
  unknownActions_.erase(it);
  return 0;
}

Action* ActionRegistry::findAction(const QString& desc) const
{
  assertActionsByKeyValid_();
  Action* action = findWithoutAliases_(desc);
  if (action != nullptr)
    return action;

  QMap<QString, QString>::const_iterator it = aliases_.find(desc);
  if (it != aliases_.end())
    return findWithoutAliases_(*it);

  return nullptr;
}

Action* ActionRegistry::findWithoutAliases_(const QString& desc) const
{
  QMap<QString, Action*>::const_iterator i = actionsByDesc_.find(desc);
  return (i == actionsByDesc_.end()) ? nullptr : *i;
}

Action* ActionRegistry::findAction(QKeySequence hotKey) const
{
  QMap<QKeySequence, Action*>::const_iterator i = actionsByKey_.find(hotKey);
  return (i == actionsByKey_.end()) ? nullptr : *i;
}

QList<Action*> ActionRegistry::actions() const
{
  assertActionsByKeyValid_();
  QList<Action*> actions;
  for (auto it = actionsByDesc_.begin(); it != actionsByDesc_.end(); ++it)
    actions.push_back(*it);
  return actions;
}

int ActionRegistry::removeHotKey(Action* action, unsigned int bindingNum)
{
  if (action == nullptr)
    return 1;
  QList<QKeySequence> newKeys = action->hotkeys();
  if (bindingNum < static_cast<unsigned int>(newKeys.size()))
  {
    newKeys.removeAt(bindingNum);
    return setHotKeys(action, newKeys);
  }
  // Error: bindingNum is too high
  return 1;
}

int ActionRegistry::setHotKey(Action* action, QKeySequence hotkey)
{
  if (action == nullptr)
    return 1;
  QList<QKeySequence> newKeys;
  newKeys += hotkey;
  return setHotKeys(action, newKeys);
}

int ActionRegistry::addHotKey(const QString& actionDesc, QKeySequence hotkey)
{
  Action* action = findAction(actionDesc);
  if (action != nullptr)
  {
    QList<QKeySequence> newKeys = action->hotkeys();
    newKeys += hotkey;
    return setHotKeys(action, newKeys);
  }

  // Find the action by hot key; it needs to remove that hot key
  action = findAction(hotkey);
  // If it exists, remove the hot key
  if (action)
  {
    const int whichKey = action->hotkeys().indexOf(hotkey);
    // Assertion failure means findAction() failed or hotkeys() isn't in sync
    assert(whichKey >= 0);
    if (whichKey >= 0)
      removeHotKey(action, whichKey);
  }

  // If that hot key is in use in the unknowns list, update that too
  auto keyIter = unknownActionsByKey_.find(hotkey);
  if (keyIter != unknownActionsByKey_.end())
  {
    auto unkIter = unknownActions_.find(keyIter.value());
    // Out of sync error
    assert(unkIter != unknownActions_.end());
    if (unkIter != unknownActions_.end())
      unkIter.value()->hotkeys.removeAll(hotkey);
    unknownActionsByKey_.erase(keyIter);
  }

  // Save as an unknown action, store hotkey for later
  QMap<QString, UnknownAction*>::const_iterator i = unknownActions_.find(actionDesc);
  UnknownAction* unknown = nullptr;
  if (i == unknownActions_.end())
  {
    unknown = new UnknownAction;
    unknown->description = actionDesc;
    unknownActions_[actionDesc] = unknown;
  }
  else
    unknown = *i;
  unknown->hotkeys += hotkey;

  // Register the unknown key
  unknownActionsByKey_[hotkey] = actionDesc;

  // Internal consistency should be rock solid here
  assertActionsByKeyValid_();
  return 0;
}

int ActionRegistry::setHotKeys(Action* action, const QList<QKeySequence>& hotkeys)
{
  if (action == nullptr)
    return 1;
  const QList<QKeySequence> uniqueHotkeys = makeUnique_(hotkeys);

  // Forget the old hotkeys in the action requested (they are going to be replaced)
  auto actionHotkeys = action->hotkeys();
  for (auto it = actionHotkeys.begin(); it != actionHotkeys.end(); ++it)
  {
    // Forget the hotkey, but don't remove it from the QAction (doing so causes recursion and is unnecessary)
    removeBinding_(action, *it, false);
  }

  // Remove the hotkey from other actions
  for (auto it = uniqueHotkeys.begin(); it != uniqueHotkeys.end(); ++it)
  {
    QKeySequence key = *it;
    // We do not need to remove the binding for our own action (no-op)
    Action* oldAction = findAction(key);
    if (oldAction != nullptr && action != oldAction)
      removeBinding_(oldAction, key, true);
    // Store association of binding to new action (unconditionally)
    actionsByKey_[key] = action;

    // Make sure to remove it from registered list of unknown actions, which requires updating that list
    auto i = unknownActionsByKey_.find(key);
    if (i != unknownActionsByKey_.end())
    {
      auto acIter = unknownActions_.find(i.value());
      // Assertion failure means there's code that makes these two lists out of sync
      assert(acIter != unknownActions_.end());
      if (acIter != unknownActions_.end())
      {
        acIter.value()->hotkeys.removeAll(key);
      }
      unknownActionsByKey_.erase(i);
    }
  }

  // Update the actual QAction
  if (action->action())
    action->action()->setShortcuts(uniqueHotkeys);
  // Assertion failure means setShortcuts() failed in an unknown way
  assert(uniqueHotkeys == action->hotkeys());
  // Assertion failure means our hotkeys list lost sync with action-by-key map
  assert(actionsByKey_.values().count(action) == uniqueHotkeys.size());
  Q_EMIT(hotKeysChanged(action));

  // Make sure internal consistency is correct
  assertActionsByKeyValid_();
  return 0;
}

ActionRegistry::SettingsMemento* ActionRegistry::createMemento() const
{
  return new MementoImpl(*this);
}

QList<QKeySequence> ActionRegistry::takeUnknown_(const QString &actionDesc)
{
  QMap<QString, UnknownAction*>::iterator i = unknownActions_.find(actionDesc);
  QList<QKeySequence> rv;
  if (i != unknownActions_.end())
  {
    rv = i.value()->hotkeys;
    delete i.value();
    unknownActions_.erase(i);
  }

  // Remove each hot key from the unknownActionsByKey_ list
  for (auto j = rv.begin(); j != rv.end(); ++j)
    unknownActionsByKey_.remove(*j);
  return rv;
}

void ActionRegistry::combineAndSetKeys_(Action* action, const QList<QKeySequence>& originalKeys, const QList<QKeySequence>& unknownKeys)
{
  QList<QKeySequence> allKeys;
  // Only permit a key to be set during initialization if it's not already used (don't override)
  // Also, unknown keys take priority simply because they're from the user, instead of initial defaults
  for (auto it = unknownKeys.begin(); it != unknownKeys.end(); ++it)
  {
    if (!actionsByKey_.contains(*it))
      allKeys.push_back(*it);
  }
  for (auto it = originalKeys.begin(); it != originalKeys.end(); ++it)
  {
    if (!actionsByKey_.contains(*it))
      allKeys.push_back(*it);
  }
  // Update the hotkeys
  setHotKeys(action, allKeys);
}

int ActionRegistry::removeBinding_(Action* fromAction, QKeySequence key, bool updateQAction)
{
  if (fromAction == nullptr || !fromAction->action())
    return 1;
  QMap<QKeySequence, Action*>::iterator i = actionsByKey_.find(key);
  if (i != actionsByKey_.end())
  {
    // It's possible that i.value() is not fromAction.  This can occur if a new action is
    // being registered that has a hotkey association internally (via QAction::shortcut),
    // but we are already using that hotkey.  In this case, we do not remove the iterator
    if (i.value() == fromAction)
      actionsByKey_.erase(i);
  }
  // Remove it from the list of keys in the QAction
  if (updateQAction)
  {
    QList<QKeySequence> newKeys = fromAction->hotkeys();
    newKeys.removeOne(key);
    fromAction->action()->setShortcuts(newKeys);
    Q_EMIT(hotKeysChanged(fromAction));
    Q_EMIT(hotKeyLost(fromAction, key));
  }
  return 0;
}

int ActionRegistry::serialize(QSettings& settings, const QString& groupName) const
{
  if (settings.status() != QSettings::NoError)
    return 1;
  return MementoImpl::serializeTo(*this, settings, groupName);
}

int ActionRegistry::serialize(const QString& filename, const QString& groupName) const
{
  QSettings settings(filename, QSettings::IniFormat);
  return serialize(settings, groupName);
}

int ActionRegistry::deserialize(QSettings& settings, const QString &groupName)
{
  if (settings.status() != QSettings::NoError)
    return 1;
  return MementoImpl::deserializeFrom(*this, settings, groupName);
}

int ActionRegistry::deserialize(const QString& filename, const QString &groupName)
{
  QFileInfo fi(filename);
  if (!fi.isFile())
    return 1;
  QSettings settings(filename, QSettings::IniFormat);
  return deserialize(settings, groupName);
}

QList<QKeySequence> ActionRegistry::makeUnique_(const QList<QKeySequence>& keys) const
{
  QList<QKeySequence> rv;
  for (auto it = keys.begin(); it != keys.end(); ++it)
  {
    if (!rv.contains(*it))
      rv.push_back(*it);
  }
  return rv;
}

ActionRegistry::AssignmentStatus ActionRegistry::getKeySequenceAssignment(const QKeySequence& hotKey, QString& actionName) const
{
  // Search known hot keys first
  auto actionIter = actionsByKey_.find(hotKey);
  if (actionIter != actionsByKey_.end())
  {
    actionName = actionIter.value()->description();
    return ASSIGNED_TO_ACTION;
  }

  // Search unknown hot keys
  auto unkIter = unknownActionsByKey_.find(hotKey);
  if (unkIter != unknownActionsByKey_.end())
  {
    actionName = unkIter.value();
    return ASSIGNED_TO_UNKNOWN;
  }

  // Not found
  actionName.clear();
  return UNASSIGNED;
}

}

