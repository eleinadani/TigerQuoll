/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Jono DiCarlo <jdicarlo@mozilla.org>
 *  Anant Narayanan <anant@kix.in>
 *  Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ['BookmarksEngine', 'BookmarksSharingManager'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const GUID_ANNO = "sync/guid";
const PARENT_ANNO = "sync/parent";
const PREDECESSOR_ANNO = "sync/predecessor";
const SERVICE_NOT_SUPPORTED = "Service not supported on this platform";
const FOLDER_SORTINDEX = 1000000;

try {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
}
catch(ex) {
  Cu.import("resource://gre/modules/utils.js");
}
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/engines.js");
Cu.import("resource://services-sync/stores.js");
Cu.import("resource://services-sync/trackers.js");
Cu.import("resource://services-sync/type_records/bookmark.js");
Cu.import("resource://services-sync/util.js");

function archiveBookmarks() {
  // Some nightly builds of 3.7 don't have this function
  try {
    PlacesUtils.archiveBookmarksFile(null, true);
  }
  catch(ex) {}
}

// Lazily initialize the special top level folders
let kSpecialIds = {};
[["menu", "bookmarksMenuFolder"],
 ["places", "placesRoot"],
 ["tags", "tagsFolder"],
 ["toolbar", "toolbarFolder"],
 ["unfiled", "unfiledBookmarksFolder"],
].forEach(function([guid, placeName]) {
  Utils.lazy2(kSpecialIds, guid, function() Svc.Bookmark[placeName]);
});
Utils.lazy2(kSpecialIds, "mobile", function() {
  // Use the (one) mobile root if it already exists
  let anno = "mobile/bookmarksRoot";
  let root = Svc.Annos.getItemsWithAnnotation(anno, {});
  if (root.length != 0)
    return root[0];

  // Create the special mobile folder to store mobile bookmarks
  let mobile = Svc.Bookmark.createFolder(Svc.Bookmark.placesRoot, "mobile", -1);
  Utils.anno(mobile, anno, 1);
  return mobile;
});

function BookmarksEngine() {
  SyncEngine.call(this, "Bookmarks");
  this._handleImport();
}
BookmarksEngine.prototype = {
  __proto__: SyncEngine.prototype,
  _recordObj: PlacesItem,
  _storeObj: BookmarksStore,
  _trackerObj: BookmarksTracker,
  version: 2,

  _handleImport: function _handleImport() {
    Svc.Obs.add("bookmarks-restore-begin", function() {
      this._log.debug("Ignoring changes from importing bookmarks");
      this._tracker.ignoreAll = true;
    }, this);

    Svc.Obs.add("bookmarks-restore-success", function() {
      this._log.debug("Tracking all items on successful import");
      this._tracker.ignoreAll = false;

      // Mark all the items as changed so they get uploaded
      for (let id in this._store.getAllIDs())
        this._tracker.addChangedID(id);
    }, this);

    Svc.Obs.add("bookmarks-restore-failed", function() {
      this._tracker.ignoreAll = false;
    }, this);
  },

  _sync: Utils.batchSync("Bookmark", SyncEngine),

  _syncStartup: function _syncStart() {
    SyncEngine.prototype._syncStartup.call(this);

    // For first-syncs, make a backup for the user to restore
    if (this.lastSync == 0)
      archiveBookmarks();

    // Lazily create a mapping of folder titles and separator positions to GUID
    this.__defineGetter__("_lazyMap", function() {
      delete this._lazyMap;

      let lazyMap = {};
      for (let guid in this._store.getAllIDs()) {
        // Figure out what key to store the mapping
        let key;
        let id = this._store.idForGUID(guid);
        switch (Svc.Bookmark.getItemType(id)) {
          case Svc.Bookmark.TYPE_BOOKMARK:
            key = "b" + Svc.Bookmark.getBookmarkURI(id).spec + ":" +
              Svc.Bookmark.getItemTitle(id);
            break;
          case Svc.Bookmark.TYPE_FOLDER:
            key = "f" + Svc.Bookmark.getItemTitle(id);
            break;
          case Svc.Bookmark.TYPE_SEPARATOR:
            key = "s" + Svc.Bookmark.getItemIndex(id);
            break;
          default:
            continue;
        }

        // The mapping is on a per parent-folder-name basis
        let parent = Svc.Bookmark.getFolderIdForItem(id);
        let parentName = Svc.Bookmark.getItemTitle(parent);
        if (lazyMap[parentName] == null)
          lazyMap[parentName] = {};

        // If the entry already exists, remember that there are explicit dupes
        let entry = new String(guid);
        entry.hasDupe = lazyMap[parentName][key] != null;

        // Remember this item's guid for its parent-name/key pair
        lazyMap[parentName][key] = entry;
        this._log.trace("Mapped: " + [parentName, key, entry, entry.hasDupe]);
      }

      // Expose a helper function to get a dupe guid for an item
      return this._lazyMap = function(item) {
        // Figure out if we have something to key with
        let key;
        switch (item.type) {
          case "bookmark":
          case "query":
          case "microsummary":
            key = "b" + item.bmkUri + ":" + item.title;
            break;
          case "folder":
          case "livemark":
            key = "f" + item.title;
            break;
          case "separator":
            key = "s" + item.pos;
            break;
          default:
            return;
        }

        // Give the guid if we have the matching pair
        this._log.trace("Finding mapping: " + item.parentName + ", " + key);
        let parent = lazyMap[item.parentName];
        let dupe = parent && parent[key];
        this._log.trace("Mapped dupe: " + dupe);
        return dupe;
      };
    });
  },

  _syncFinish: function _syncFinish() {
    SyncEngine.prototype._syncFinish.call(this);
    delete this._lazyMap;
    this._tracker._ensureMobileQuery();
  },

  _createRecord: function _createRecord(id) {
    // Create the record like normal but mark it as having dupes if necessary
    let record = SyncEngine.prototype._createRecord.call(this, id);
    let entry = this._lazyMap(record);
    if (entry != null && entry.hasDupe)
      record.hasDupe = true;
    return record;
  },

  _findDupe: function _findDupe(item) {
    // Don't bother finding a dupe if the incoming item has duplicates
    if (item.hasDupe)
      return;
    return this._lazyMap(item);
  },

  _handleDupe: function _handleDupe(item, dupeId) {
    // The local dupe has the lower id, so make it the winning id
    if (dupeId < item.id)
      [item.id, dupeId] = [dupeId, item.id];

    // Trigger id change from dupe to winning and update the server
    this._store.changeItemID(dupeId, item.id);
    this._deleteId(dupeId);
    this._tracker.addChangedID(item.id, 0);
  }
};

function BookmarksStore(name) {
  Store.call(this, name);

  // Explicitly nullify our references to our cached services so we don't leak
  Svc.Obs.add("places-shutdown", function() {
    this.__bms = null;
    this.__hsvc = null;
    this.__ls = null;
    this.__ms = null;
    this.__ts = null;
    for each ([query, stmt] in Iterator(this._stmts))
      stmt.finalize();
  }, this);
}
BookmarksStore.prototype = {
  __proto__: Store.prototype,

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __hsvc: null,
  get _hsvc() {
    if (!this.__hsvc)
      this.__hsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                    getService(Ci.nsINavHistoryService).
                    QueryInterface(Ci.nsPIPlacesDatabase);
    return this.__hsvc;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
                  getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  __ms: null,
  get _ms() {
    if (!this.__ms) {
      try {
        this.__ms = Cc["@mozilla.org/microsummary/service;1"].
                    getService(Ci.nsIMicrosummaryService);
      } catch (e) {
        this._log.warn("Could not load microsummary service");
        this._log.debug(e);
        // Redefine our getter so we won't keep trying to get the service
        this.__defineGetter__("_ms", function() null);
      }
    }
    return this.__ms;
  },

  __ts: null,
  get _ts() {
    if (!this.__ts)
      this.__ts = Cc["@mozilla.org/browser/tagging-service;1"].
                  getService(Ci.nsITaggingService);
    return this.__ts;
  },


  itemExists: function BStore_itemExists(id) {
    return this.idForGUID(id) > 0;
  },

  // Hash of old GUIDs to the new renamed GUIDs
  aliases: {},

  applyIncoming: function BStore_applyIncoming(record) {
    // Ignore (accidental?) root changes
    if (record.id in kSpecialIds) {
      this._log.debug("Skipping change to root node: " + record.id);
      return;
    }

    // Convert GUID fields to the aliased GUID if necessary
    ["id", "parentid", "predecessorid"].forEach(function(field) {
      let alias = this.aliases[record[field]];
      if (alias != null)
        record[field] = alias;
    }, this);

    // Preprocess the record before doing the normal apply
    switch (record.type) {
      case "query": {
        // Convert the query uri if necessary
        if (record.bmkUri == null || record.folderName == null)
          break;

        // Tag something so that the tag exists
        let tag = record.folderName;
        let dummyURI = Utils.makeURI("about:weave#BStore_preprocess");
        this._ts.tagURI(dummyURI, [tag]);

        // Look for the id of the tag (that might have just been added)
        let tags = this._getNode(this._bms.tagsFolder);
        if (!(tags instanceof Ci.nsINavHistoryQueryResultNode))
          break;

        tags.containerOpen = true;
        for (let i = 0; i < tags.childCount; i++) {
          let child = tags.getChild(i);
          // Found the tag, so fix up the query to use the right id
          if (child.title == tag) {
            this._log.debug("query folder: " + tag + " = " + child.itemId);
            record.bmkUri = record.bmkUri.replace(/([:&]folder=)\d+/, "$1" +
              child.itemId);
            break;
          }
        }
        break;
      }
    }

    // Figure out the local id of the parent GUID if available
    let parentGUID = record.parentid;
    record._orphan = false;
    if (parentGUID != null) {
      let parentId = this.idForGUID(parentGUID);

      // Default to unfiled if we don't have the parent yet
      if (parentId <= 0) {
        this._log.trace("Reparenting to unfiled until parent is synced");
        record._orphan = true;
        parentId = kSpecialIds.unfiled;
      }

      // Save the parent id for modifying the bookmark later
      record._parent = parentId;
    }

    // Default to append unless we're not an orphan with the predecessor
    let predGUID = record.predecessorid;
    record._insertPos = Svc.Bookmark.DEFAULT_INDEX;
    if (!record._orphan) {
      // No predecessor means it's the first item
      if (predGUID == null)
        record._insertPos = 0;
      else {
        // The insert position is one after the predecessor of the same parent
        let predId = this.idForGUID(predGUID);
        if (predId != -1 && this._getParentGUIDForId(predId) == parentGUID) {
          record._insertPos = Svc.Bookmark.getItemIndex(predId) + 1;
          record._predId = predId;
        }
        else
          this._log.trace("Appending to end until predecessor is synced");
      }
    }

    // Do the normal processing of incoming records
    Store.prototype.applyIncoming.apply(this, arguments);

    // Do some post-processing if we have an item
    let itemId = this.idForGUID(record.id);
    if (itemId > 0) {
      // Move any children that are looking for this folder as a parent
      if (record.type == "folder")
        this._reparentOrphans(itemId);

      // Create an annotation to remember that it needs a parent
      if (record._orphan)
        Utils.anno(itemId, PARENT_ANNO, parentGUID);
      // It's now in the right folder, so move annotated items behind this
      else
        this._attachFollowers(itemId);

      // Create an annotation if we have a predecessor but no position
      if (predGUID != null && record._insertPos == Svc.Bookmark.DEFAULT_INDEX)
        Utils.anno(itemId, PREDECESSOR_ANNO, predGUID);
    }
  },

  /**
   * Find all ids of items that have a given value for an annotation
   */
  _findAnnoItems: function BStore__findAnnoItems(anno, val) {
    return Svc.Annos.getItemsWithAnnotation(anno, {}).filter(function(id)
      Utils.anno(id, anno) == val);
  },

  /**
   * For the provided parent item, attach its children to it
   */
  _reparentOrphans: function _reparentOrphans(parentId) {
    // Find orphans and reunite with this folder parent
    let parentGUID = this.GUIDForId(parentId);
    let orphans = this._findAnnoItems(PARENT_ANNO, parentGUID);

    this._log.debug("Reparenting orphans " + orphans + " to " + parentId);
    orphans.forEach(function(orphan) {
      // Append the orphan under the parent unless it's supposed to be first
      let insertPos = Svc.Bookmark.DEFAULT_INDEX;
      if (!Svc.Annos.itemHasAnnotation(orphan, PREDECESSOR_ANNO))
        insertPos = 0;

      // Move the orphan to the parent and drop the missing parent annotation
      Svc.Bookmark.moveItem(orphan, parentId, insertPos);
      Svc.Annos.removeItemAnnotation(orphan, PARENT_ANNO);
    });

    // Fix up the ordering of the now-parented items
    orphans.forEach(this._attachFollowers, this);
  },

  /**
   * Move an item and all of its followers to a new position until reaching an
   * item that shouldn't be moved
   */
  _moveItemChain: function BStore__moveItemChain(itemId, insertPos, stopId) {
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);

    // Keep processing the item chain until it loops to the stop item
    do {
      // Figure out what's next in the chain
      let itemPos = Svc.Bookmark.getItemIndex(itemId);
      let nextId = Svc.Bookmark.getIdForItemAt(parentId, itemPos + 1);

      Svc.Bookmark.moveItem(itemId, parentId, insertPos);
      this._log.trace("Moved " + itemId + " to " + insertPos);

      // Prepare for the next item in the chain
      insertPos = Svc.Bookmark.getItemIndex(itemId) + 1;
      itemId = nextId;

      // Stop if we ran off the end or the item is looking for its pred.
      if (itemId == -1 || Svc.Annos.itemHasAnnotation(itemId, PREDECESSOR_ANNO))
        break;
    } while (itemId != stopId);
  },

  /**
   * For the provided predecessor item, attach its followers to it
   */
  _attachFollowers: function BStore__attachFollowers(predId) {
    let predGUID = this.GUIDForId(predId);
    let followers = this._findAnnoItems(PREDECESSOR_ANNO, predGUID);
    if (followers.length > 1)
      this._log.warn(predId + " has more than one followers: " + followers);

    // Start at the first follower and move the chain of followers
    let parent = Svc.Bookmark.getFolderIdForItem(predId);
    followers.forEach(function(follow) {
      this._log.trace("Repositioning " + follow + " behind " + predId);
      if (Svc.Bookmark.getFolderIdForItem(follow) != parent) {
        this._log.warn("Follower doesn't have the same parent: " + parent);
        return;
      }

      // Move the chain of followers to after the predecessor
      let insertPos = Svc.Bookmark.getItemIndex(predId) + 1;
      this._moveItemChain(follow, insertPos, predId);

      // Remove the annotation now that we're putting it in the right spot
      Svc.Annos.removeItemAnnotation(follow, PREDECESSOR_ANNO);
    }, this);
  },

  create: function BStore_create(record) {
    let newId;
    switch (record.type) {
    case "bookmark":
    case "query":
    case "microsummary": {
      let uri = Utils.makeURI(record.bmkUri);
      newId = this._bms.insertBookmark(record._parent, uri, record._insertPos,
        record.title);
      this._log.debug(["created bookmark", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title, record.bmkUri].join(" "));

      this._tagURI(uri, record.tags);
      this._bms.setKeywordForBookmark(newId, record.keyword);
      if (record.description)
        Utils.anno(newId, "bookmarkProperties/description", record.description);

      if (record.loadInSidebar)
        Utils.anno(newId, "bookmarkProperties/loadInSidebar", true);

      if (record.type == "microsummary") {
        this._log.debug("   \-> is a microsummary");
        Utils.anno(newId, "bookmarks/staticTitle", record.staticTitle || "");
        let genURI = Utils.makeURI(record.generatorUri);
        if (this._ms) {
          try {
            let micsum = this._ms.createMicrosummary(uri, genURI);
            this._ms.setMicrosummary(newId, micsum);
          }
          catch(ex) { /* ignore "missing local generator" exceptions */ }
        }
        else
          this._log.warn("Can't create microsummary -- not supported.");
      }
    } break;
    case "folder":
      newId = this._bms.createFolder(record._parent, record.title,
        record._insertPos);
      this._log.debug(["created folder", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title].join(" "));

      if (record.description)
        Utils.anno(newId, "bookmarkProperties/description", record.description);
      break;
    case "livemark":
      let siteURI = null;
      if (record.siteUri != null)
        siteURI = Utils.makeURI(record.siteUri);

      newId = this._ls.createLivemark(record._parent, record.title, siteURI,
        Utils.makeURI(record.feedUri), record._insertPos);
      this._log.debug(["created livemark", newId, "under", record._parent, "at",
        record._insertPos, "as", record.title, record.siteUri, record.feedUri].
        join(" "));
      break;
    case "separator":
      newId = this._bms.insertSeparator(record._parent, record._insertPos);
      this._log.debug(["created separator", newId, "under", record._parent,
        "at", record._insertPos].join(" "));
      break;
    case "item":
      this._log.debug(" -> got a generic places item.. do nothing?");
      return;
    default:
      this._log.error("_create: Unknown item type: " + record.type);
      return;
    }

    this._log.trace("Setting GUID of new item " + newId + " to " + record.id);
    this._setGUID(newId, record.id);
  },

  remove: function BStore_remove(record) {
    let itemId = this.idForGUID(record.id);
    if (itemId <= 0) {
      this._log.debug("Item " + record.id + " already removed");
      return;
    }
    var type = this._bms.getItemType(itemId);

    switch (type) {
    case this._bms.TYPE_BOOKMARK:
      this._log.debug("  -> removing bookmark " + record.id);
      this._ts.untagURI(this._bms.getBookmarkURI(itemId), null);
      this._bms.removeItem(itemId);
      break;
    case this._bms.TYPE_FOLDER:
      this._log.debug("  -> removing folder " + record.id);
      Svc.Bookmark.removeItem(itemId);
      break;
    case this._bms.TYPE_SEPARATOR:
      this._log.debug("  -> removing separator " + record.id);
      this._bms.removeItem(itemId);
      break;
    default:
      this._log.error("remove: Unknown item type: " + type);
      break;
    }
  },

  update: function BStore_update(record) {
    let itemId = this.idForGUID(record.id);

    if (itemId <= 0) {
      this._log.debug("Skipping update for unknown item: " + record.id);
      return;
    }

    this._log.trace("Updating " + record.id + " (" + itemId + ")");

    // Move the bookmark to a new parent if necessary
    if (Svc.Bookmark.getFolderIdForItem(itemId) != record._parent) {
      this._log.trace("Moving item to a new parent");
      Svc.Bookmark.moveItem(itemId, record._parent, record._insertPos);
    }
    // Move the chain of bookmarks to a new position
    else if (Svc.Bookmark.getItemIndex(itemId) != record._insertPos &&
             !record._orphan) {
      this._log.trace("Moving item and followers to a new position");

      // Stop moving at the predecessor unless we don't have one
      this._moveItemChain(itemId, record._insertPos, record._predId || itemId);
    }

    for (let [key, val] in Iterator(record.cleartext)) {
      switch (key) {
      case "title":
        this._bms.setItemTitle(itemId, val);
        break;
      case "bmkUri":
        this._bms.changeBookmarkURI(itemId, Utils.makeURI(val));
        break;
      case "tags":
        this._tagURI(this._bms.getBookmarkURI(itemId), val);
        break;
      case "keyword":
        this._bms.setKeywordForBookmark(itemId, val);
        break;
      case "description":
        Utils.anno(itemId, "bookmarkProperties/description", val);
        break;
      case "loadInSidebar":
        if (val)
          Utils.anno(itemId, "bookmarkProperties/loadInSidebar", true);
        else
          Svc.Annos.removeItemAnnotation(itemId, "bookmarkProperties/loadInSidebar");
        break;
      case "generatorUri": {
        try {
          let micsumURI = this._bms.getBookmarkURI(itemId);
          let genURI = Utils.makeURI(val);
          if (this._ms == SERVICE_NOT_SUPPORTED)
            this._log.warn("Can't create microsummary -- not supported.");
          else {
            let micsum = this._ms.createMicrosummary(micsumURI, genURI);
            this._ms.setMicrosummary(itemId, micsum);
          }
        } catch (e) {
          this._log.debug("Could not set microsummary generator URI: " + e);
        }
      } break;
      case "siteUri":
        this._ls.setSiteURI(itemId, Utils.makeURI(val));
        break;
      case "feedUri":
        this._ls.setFeedURI(itemId, Utils.makeURI(val));
        break;
      }
    }
  },

  changeItemID: function BStore_changeItemID(oldID, newID) {
    // Remember the GUID change for incoming records
    this.aliases[oldID] = newID;

    // Update any existing annotation references
    this._findAnnoItems(PARENT_ANNO, oldID).forEach(function(itemId) {
      Utils.anno(itemId, PARENT_ANNO, newID);
    }, this);
    this._findAnnoItems(PREDECESSOR_ANNO, oldID).forEach(function(itemId) {
      Utils.anno(itemId, PREDECESSOR_ANNO, newID);
    }, this);

    // Make sure there's an item to change GUIDs
    let itemId = this.idForGUID(oldID);
    if (itemId <= 0)
      return;

    this._log.debug("Changing GUID " + oldID + " to " + newID);
    this._setGUID(itemId, newID);
  },

  _getNode: function BStore__getNode(folder) {
    let query = this._hsvc.getNewQuery();
    query.setFolders([folder], 1);
    return this._hsvc.executeQuery(query, this._hsvc.getNewQueryOptions()).root;
  },

  _getTags: function BStore__getTags(uri) {
    try {
      if (typeof(uri) == "string")
        uri = Utils.makeURI(uri);
    } catch(e) {
      this._log.warn("Could not parse URI \"" + uri + "\": " + e);
    }
    return this._ts.getTagsForURI(uri, {});
  },

  _getDescription: function BStore__getDescription(id) {
    try {
      return Utils.anno(id, "bookmarkProperties/description");
    } catch (e) {
      return undefined;
    }
  },

  _isLoadInSidebar: function BStore__isLoadInSidebar(id) {
    return Svc.Annos.itemHasAnnotation(id, "bookmarkProperties/loadInSidebar");
  },

  _getStaticTitle: function BStore__getStaticTitle(id) {
    try {
      return Utils.anno(id, "bookmarks/staticTitle");
    } catch (e) {
      return "";
    }
  },

  // Create a record starting from the weave id (places guid)
  createRecord: function createRecord(id, collection) {
    let placeId = this.idForGUID(id);
    let record;
    if (placeId <= 0) { // deleted item
      record = new PlacesItem(collection, id);
      record.deleted = true;
      return record;
    }

    let parent = Svc.Bookmark.getFolderIdForItem(placeId);
    switch (this._bms.getItemType(placeId)) {
    case this._bms.TYPE_BOOKMARK:
      let bmkUri = this._bms.getBookmarkURI(placeId).spec;
      if (this._ms && this._ms.hasMicrosummary(placeId)) {
        record = new BookmarkMicsum(collection, id);
        let micsum = this._ms.getMicrosummary(placeId);
        record.generatorUri = micsum.generator.uri.spec; // breaks local generators
        record.staticTitle = this._getStaticTitle(placeId);
      }
      else {
        if (bmkUri.search(/^place:/) == 0) {
          record = new BookmarkQuery(collection, id);

          // Get the actual tag name instead of the local itemId
          let folder = bmkUri.match(/[:&]folder=(\d+)/);
          try {
            // There might not be the tag yet when creating on a new client
            if (folder != null) {
              folder = folder[1];
              record.folderName = this._bms.getItemTitle(folder);
              this._log.debug("query id: " + folder + " = " + record.folderName);
            }
          }
          catch(ex) {}
        }
        else
          record = new Bookmark(collection, id);
        record.title = this._bms.getItemTitle(placeId);
      }

      record.parentName = Svc.Bookmark.getItemTitle(parent);
      record.bmkUri = bmkUri;
      record.tags = this._getTags(record.bmkUri);
      record.keyword = this._bms.getKeywordForBookmark(placeId);
      record.description = this._getDescription(placeId);
      record.loadInSidebar = this._isLoadInSidebar(placeId);
      break;

    case this._bms.TYPE_FOLDER:
      if (this._ls.isLivemark(placeId)) {
        record = new Livemark(collection, id);

        let siteURI = this._ls.getSiteURI(placeId);
        if (siteURI != null)
          record.siteUri = siteURI.spec;
        record.feedUri = this._ls.getFeedURI(placeId).spec;

      } else {
        record = new BookmarkFolder(collection, id);
      }

      record.parentName = Svc.Bookmark.getItemTitle(parent);
      record.title = this._bms.getItemTitle(placeId);
      record.description = this._getDescription(placeId);
      break;

    case this._bms.TYPE_SEPARATOR:
      record = new BookmarkSeparator(collection, id);
      // Create a positioning identifier for the separator
      record.parentName = Svc.Bookmark.getItemTitle(parent);
      record.pos = Svc.Bookmark.getItemIndex(placeId);
      break;

    case this._bms.TYPE_DYNAMIC_CONTAINER:
      record = new PlacesItem(collection, id);
      this._log.warn("Don't know how to serialize dynamic containers yet");
      break;

    default:
      record = new PlacesItem(collection, id);
      this._log.warn("Unknown item type, cannot serialize: " +
                     this._bms.getItemType(placeId));
    }

    record.parentid = this._getParentGUIDForId(placeId);
    record.predecessorid = this._getPredecessorGUIDForId(placeId);
    record.sortindex = this._calculateIndex(record);

    return record;
  },

  _stmts: {},
  _getStmt: function(query) {
    if (query in this._stmts)
      return this._stmts[query];

    this._log.trace("Creating SQL statement: " + query);
    return this._stmts[query] = Utils.createStatement(this._hsvc.DBConnection,
                                                      query);
  },

  get _frecencyStm() {
    return this._getStmt(
        "SELECT frecency " +
        "FROM moz_places " +
        "WHERE url = :url " +
        "LIMIT 1");
  },

  get _addGUIDAnnotationNameStm() {
    let stmt = this._getStmt(
      "INSERT OR IGNORE INTO moz_anno_attributes (name) VALUES (:anno_name)");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _checkGUIDItemAnnotationStm() {
    let stmt = this._getStmt(
      "SELECT b.id AS item_id, " +
        "(SELECT id FROM moz_anno_attributes WHERE name = :anno_name) AS name_id, " +
        "a.id AS anno_id, a.dateAdded AS anno_date " +
      "FROM moz_bookmarks b " +
      "LEFT JOIN moz_items_annos a ON a.item_id = b.id " +
                                 "AND a.anno_attribute_id = name_id " +
      "WHERE b.id = :item_id");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  get _addItemAnnotationStm() {
    return this._getStmt(
    "INSERT OR REPLACE INTO moz_items_annos " +
      "(id, item_id, anno_attribute_id, mime_type, content, flags, " +
       "expiration, type, dateAdded, lastModified) " +
    "VALUES (:id, :item_id, :name_id, :mime_type, :content, :flags, " +
            ":expiration, :type, :date_added, :last_modified)");
  },

  // Some helper functions to handle GUIDs
  _setGUID: function _setGUID(id, guid) {
    if (arguments.length == 1)
      guid = Utils.makeGUID();

    // Ensure annotation name exists
    Utils.queryAsync(this._addGUIDAnnotationNameStm);

    let stmt = this._checkGUIDItemAnnotationStm;
    stmt.params.item_id = id;
    let result = Utils.queryAsync(stmt, ["item_id", "name_id", "anno_id",
                                         "anno_date"])[0];
    if (!result) {
      let log = Log4Moz.repository.getLogger("Engine.Bookmarks");
      log.warn("Couldn't annotate bookmark id " + id);
      return guid;
    }

    stmt = this._addItemAnnotationStm;
    if (result.anno_id) {
      stmt.params.id = result.anno_id;
      stmt.params.date_added = result.anno_date;
    } else {
      stmt.params.id = null;
      stmt.params.date_added = Date.now() * 1000;
    }
    stmt.params.item_id = result.item_id;
    stmt.params.name_id = result.name_id;
    stmt.params.content = guid;
    stmt.params.flags = 0;
    stmt.params.expiration = Ci.nsIAnnotationService.EXPIRE_NEVER;
    stmt.params.type = Ci.nsIAnnotationService.TYPE_STRING;
    stmt.params.last_modified = Date.now() * 1000;
    Utils.queryAsync(stmt);

    return guid;
  },

  get _guidForIdStm() {
    let stmt = this._getStmt(
      "SELECT a.content AS guid " +
      "FROM moz_items_annos a " +
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
      "JOIN moz_bookmarks b ON b.id = a.item_id " +
      "WHERE n.name = :anno_name AND b.id = :item_id");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  GUIDForId: function GUIDForId(id) {
    for (let [guid, specialId] in Iterator(kSpecialIds))
      if (id == specialId)
         return guid;

    let stmt = this._guidForIdStm;
    stmt.params.item_id = id;

    // Use the existing GUID if it exists
    let result = Utils.queryAsync(stmt, ["guid"])[0];
    if (result)
      return result.guid;

    // Give the uri a GUID if it doesn't have one
    return this._setGUID(id);
  },

  get _idForGUIDStm() {
    let stmt = this._getStmt(
      "SELECT a.item_id AS item_id " +
      "FROM moz_items_annos a " +
      "JOIN moz_anno_attributes n ON n.id = a.anno_attribute_id " +
      "WHERE n.name = :anno_name AND a.content = :guid");
    stmt.params.anno_name = GUID_ANNO;
    return stmt;
  },

  idForGUID: function idForGUID(guid) {
    if (guid in kSpecialIds)
      return kSpecialIds[guid];

    let stmt = this._idForGUIDStm;
    // guid might be a String object rather than a string.
    stmt.params.guid = guid.toString();

    let result = Utils.queryAsync(stmt, ["item_id"])[0];
    if (result)
      return result.item_id;
    return -1;
  },

  _calculateIndex: function _calculateIndex(record) {
    // Ensure folders have a very high sort index so they're not synced last.
    if (record.type == "folder")
      return FOLDER_SORTINDEX;

    // For anything directly under the toolbar, give it a boost of more than an
    // unvisited bookmark
    let index = 0;
    if (record.parentid == "toolbar")
      index += 150;

    // Add in the bookmark's frecency if we have something
    if (record.bmkUri != null) {
      this._frecencyStm.params.url = record.bmkUri;
      let result = Utils.queryAsync(this._frecencyStm, ["frecency"]);
      if (result.length)
        index += result[0].frecency;
    }

    return index;
  },

  _getParentGUIDForId: function BStore__getParentGUIDForId(itemId) {
    // Give the parent annotation if it exists
    try {
      return Utils.anno(itemId, PARENT_ANNO);
    }
    catch(ex) {}

    let parentid = this._bms.getFolderIdForItem(itemId);
    if (parentid == -1) {
      this._log.debug("Found orphan bookmark, reparenting to unfiled");
      parentid = this._bms.unfiledBookmarksFolder;
      this._bms.moveItem(itemId, parentid, -1);
    }
    return this.GUIDForId(parentid);
  },

  _getPredecessorGUIDForId: function BStore__getPredecessorGUIDForId(itemId) {
    // Give the predecessor annotation if it exists
    try {
      return Utils.anno(itemId, PREDECESSOR_ANNO);
    }
    catch(ex) {}

    // Figure out the predecessor, unless it's the first item
    let itemPos = Svc.Bookmark.getItemIndex(itemId);
    if (itemPos == 0)
      return;

    // For items directly under unfiled/unsorted, give no predecessor
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);
    if (parentId == Svc.Bookmark.unfiledBookmarksFolder)
      return;

    let predecessorId = Svc.Bookmark.getIdForItemAt(parentId, itemPos - 1);
    if (predecessorId == -1) {
      this._log.debug("No predecessor directly before " + itemId + " under " +
        parentId + " at " + itemPos);

      // Find the predecessor before the item
      do {
        // No more items to check, it must be the first one
        if (--itemPos < 0)
          break;
        predecessorId = Svc.Bookmark.getIdForItemAt(parentId, itemPos);
      } while (predecessorId == -1);

      // Fix up the item to be at the right position for next time
      itemPos++;
      this._log.debug("Fixing " + itemId + " to be at position " + itemPos);
      Svc.Bookmark.moveItem(itemId, parentId, itemPos);

      // There must be no predecessor for this item!
      if (itemPos == 0)
        return;
    }

    return this.GUIDForId(predecessorId);
  },

  _getChildren: function BStore_getChildren(guid, items) {
    let node = guid; // the recursion case
    if (typeof(node) == "string") // callers will give us the guid as the first arg
      node = this._getNode(this.idForGUID(guid));

    if (node.type == node.RESULT_TYPE_FOLDER &&
        !this._ls.isLivemark(node.itemId)) {
      node.QueryInterface(Ci.nsINavHistoryQueryResultNode);
      node.containerOpen = true;

      // Remember all the children GUIDs and recursively get more
      for (var i = 0; i < node.childCount; i++) {
        let child = node.getChild(i);
        items[this.GUIDForId(child.itemId)] = true;
        this._getChildren(child, items);
      }
    }

    return items;
  },

  _tagURI: function BStore_tagURI(bmkURI, tags) {
    // Filter out any null/undefined/empty tags
    tags = tags.filter(function(t) t);

    // Temporarily tag a dummy uri to preserve tag ids when untagging
    let dummyURI = Utils.makeURI("about:weave#BStore_tagURI");
    this._ts.tagURI(dummyURI, tags);
    this._ts.untagURI(bmkURI, null);
    this._ts.tagURI(bmkURI, tags);
    this._ts.untagURI(dummyURI, null);
  },

  getAllIDs: function BStore_getAllIDs() {
    let items = {};
    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places" && guid != "tags")
        this._getChildren(guid, items);
    return items;
  },

  wipe: function BStore_wipe() {
    // Save a backup before clearing out all bookmarks
    archiveBookmarks();

    for (let [guid, id] in Iterator(kSpecialIds))
      if (guid != "places")
        this._bms.removeFolderChildren(id);
  }
};

function BookmarksTracker(name) {
  Tracker.call(this, name);

  // Ignore changes to the special roots
  for (let guid in kSpecialIds)
    this.ignoreID(guid);

  Svc.Obs.add("places-shutdown", this);
  Svc.Obs.add("weave:engine:start-tracking", this);
  Svc.Obs.add("weave:engine:stop-tracking", this);
}
BookmarksTracker.prototype = {
  __proto__: Tracker.prototype,

  _enabled: false,
  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "weave:engine:start-tracking":
        if (!this._enabled) {
          Svc.Bookmark.addObserver(this, true);
          this._enabled = true;
        }
        break;
      case "weave:engine:stop-tracking":
        if (this._enabled) {
          Svc.Bookmark.removeObserver(this);
          this._enabled = false;
        }
        // Fall through to clean up.
      case "places-shutdown":
        // Explicitly nullify our references to our cached services so
        // we don't leak
        this.__ls = null;
        this.__bms = null;
        break;
    }
  },

  __bms: null,
  get _bms() {
    if (!this.__bms)
      this.__bms = Cc["@mozilla.org/browser/nav-bookmarks-service;1"].
                   getService(Ci.nsINavBookmarksService);
    return this.__bms;
  },

  __ls: null,
  get _ls() {
    if (!this.__ls)
      this.__ls = Cc["@mozilla.org/browser/livemark-service;2"].
                  getService(Ci.nsILivemarkService);
    return this.__ls;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsINavBookmarkObserver,
    Ci.nsINavBookmarkObserver_MOZILLA_1_9_1_ADDITIONS,
    Ci.nsISupportsWeakReference
  ]),

  _GUIDForId: function _GUIDForId(item_id) {
    // Isn't indirection fun...
    return Engines.get("bookmarks")._store.GUIDForId(item_id);
  },

  /**
   * Add a bookmark (places) id to be uploaded and bump up the sync score
   *
   * @param itemId
   *        Places internal id of the bookmark to upload
   */
  _addId: function BMT__addId(itemId) {
    if (this.addChangedID(this._GUIDForId(itemId, true)))
      this._upScore();
  },

  /**
   * Add the successor id for the item that follows the given item
   */
  _addSuccessor: function BMT__addSuccessor(itemId) {
    let parentId = Svc.Bookmark.getFolderIdForItem(itemId);
    let itemPos = Svc.Bookmark.getItemIndex(itemId);
    let succId = Svc.Bookmark.getIdForItemAt(parentId, itemPos + 1);
    if (succId != -1)
      this._addId(succId);
  },

  /* Every add/remove/change is worth 10 points */
  _upScore: function BMT__upScore() {
    this.score += 10;
  },

  /**
   * Determine if a change should be ignored: we're ignoring everything or the
   * folder is for livemarks
   *
   * @param itemId
   *        Item under consideration to ignore
   * @param folder (optional)
   *        Folder of the item being changed
   */
  _ignore: function BMT__ignore(itemId, folder) {
    // Ignore unconditionally if the engine tells us to
    if (this.ignoreAll)
      return true;

    // Ensure that the mobile bookmarks query is correct in the UI
    this._ensureMobileQuery();

    // Make sure to remove items that have the exclude annotation
    if (Svc.Annos.itemHasAnnotation(itemId, "places/excludeFromBackup")) {
      this.removeChangedID(this._GUIDForId(itemId, true));
      return true;
    }

    // Get the folder id if we weren't given one
    if (folder == null)
      folder = this._bms.getFolderIdForItem(itemId);

    let tags = kSpecialIds.tags;
    // Ignore changes to tags (folders under the tags folder)
    if (folder == tags)
      return true;

    // Ignore tag items (the actual instance of a tag for a bookmark)
    if (this._bms.getFolderIdForItem(folder) == tags)
      return true;

    // Ignore livemark children
    return this._ls.isLivemark(folder);
  },

  onItemAdded: function BMT_onEndUpdateBatch(itemId, folder, index) {
    if (this._ignore(itemId, folder))
      return;

    this._log.trace("onItemAdded: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);
  },

  onBeforeItemRemoved: function BMT_onBeforeItemRemoved(itemId) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onBeforeItemRemoved: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);
  },

  _ensureMobileQuery: function _ensureMobileQuery() {
    let anno = "PlacesOrganizer/OrganizerQuery";
    let find = function(val) Svc.Annos.getItemsWithAnnotation(anno, {}).filter(
      function(id) Utils.anno(id, anno) == val);

    // Don't continue if the Library isn't ready
    let all = find("AllBookmarks");
    if (all.length == 0)
      return;

    // Disable handling of notifications while changing the mobile query
    this.ignoreAll = true;

    let mobile = find("MobileBookmarks");
    let queryURI = Utils.makeURI("place:folder=" + kSpecialIds.mobile);
    let title = Str.sync.get("mobile.label");

    // Don't add OR do remove the mobile bookmarks if there's nothing
    if (Svc.Bookmark.getIdForItemAt(kSpecialIds.mobile, 0) == -1) {
      if (mobile.length != 0)
        Svc.Bookmark.removeItem(mobile[0]);
    }
    // Add the mobile bookmarks query if it doesn't exist
    else if (mobile.length == 0) {
      let query = Svc.Bookmark.insertBookmark(all[0], queryURI, -1, title);
      Utils.anno(query, anno, "MobileBookmarks");
      Utils.anno(query, "places/excludeFromBackup", 1);
    }
    // Make sure the existing title is correct
    else if (Svc.Bookmark.getItemTitle(mobile[0]) != title)
      Svc.Bookmark.setItemTitle(mobile[0], title);

    this.ignoreAll = false;
  },

  onItemChanged: function BMT_onItemChanged(itemId, property, isAnno, value) {
    if (this._ignore(itemId))
      return;

    // ignore annotations except for the ones that we sync
    let annos = ["bookmarkProperties/description",
      "bookmarkProperties/loadInSidebar", "bookmarks/staticTitle",
      "livemark/feedURI", "livemark/siteURI", "microsummary/generatorURI"];
    if (isAnno && annos.indexOf(property) == -1)
      return;

    // Ignore favicon changes to avoid unnecessary churn
    if (property == "favicon")
      return;

    this._log.trace("onItemChanged: " + itemId +
                    (", " + property + (isAnno? " (anno)" : "")) +
                    (value? (" = \"" + value + "\"") : ""));
    this._addId(itemId);
  },

  onItemMoved: function BMT_onItemMoved(itemId, oldParent, oldIndex, newParent, newIndex) {
    if (this._ignore(itemId))
      return;

    this._log.trace("onItemMoved: " + itemId);
    this._addId(itemId);
    this._addSuccessor(itemId);

    // Get the thing that's now at the old place
    let oldSucc = Svc.Bookmark.getIdForItemAt(oldParent, oldIndex);
    if (oldSucc != -1)
      this._addId(oldSucc);

    // Remove any position annotations now that the user moved the item
    Svc.Annos.removeItemAnnotation(itemId, PARENT_ANNO);
    Svc.Annos.removeItemAnnotation(itemId, PREDECESSOR_ANNO);
  },

  onBeginUpdateBatch: function BMT_onBeginUpdateBatch() {},
  onEndUpdateBatch: function BMT_onEndUpdateBatch() {},
  onItemRemoved: function BMT_onItemRemoved(itemId, folder, index) {},
  onItemVisited: function BMT_onItemVisited(itemId, aVisitID, time) {}
};
