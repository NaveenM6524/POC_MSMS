#ifndef INVENTORY_H
#define INVENTORY_H

#include "common.h"

void inventoryInit(void);
void inventoryFreeAll(void);

OpStatus inventoryAddNew(const char *name, const char *batch, int quantity,
                          const char *expiryDate, int reorderLevel,
                          const char *actor, int *outId);

OpStatus inventoryRemove(int id, const char *actor);

OpStatus inventoryUpdate(int id, const char *name, const char *batch,
                          int reorderLevel, const char *actor);

OpStatus inventoryIncreaseStock(int id, int qty, const char *actor);
OpStatus inventoryDecreaseStock(int id, int qty, const char *actor);

Medicine *inventoryFindById(int id);
Medicine *inventoryFindExact(const char *name, const char *batch, const char *expiryDate);

/* fills outArray (caller-owned, size maxResults) with pointers to live
 * records matching name; returns how many were written */
int inventoryFindAllByName(const char *name, Medicine **outArray, int maxResults);

/* fills outArray with pointers to every live record; returns count */
int inventoryGetAll(Medicine **outArray, int maxResults);

OpStatus inventorySave(void);

#endif
