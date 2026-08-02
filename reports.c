#include <stdio.h>
#include <string.h>

#include "reports.h"
#include "inventory.h"
#include "util.h"
#include "logging.h"
#include "common.h"

void reportStock(const char *viewer)
{
    Medicine *items[MAX_QUERY_RESULTS];
    int count = inventoryGetAll(items, MAX_QUERY_RESULTS);

    printf("\n%-5s %-24s %-12s %-8s %-12s %-8s\n",
           "ID", "Name", "Batch", "Qty", "Expiry", "Reorder");
    printf("---------------------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        printf("%-5d %-24s %-12s %-8d %-12s %-8d\n",
               items[i]->id, items[i]->name, items[i]->batch,
               items[i]->quantity, items[i]->expiryDate, items[i]->reorderLevel);
    }
    if (count == 0) {
        printf("(no inventory records)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "stock report");
}

void reportLowStock(const char *viewer)
{
    Medicine *items[MAX_QUERY_RESULTS];
    int count = inventoryGetAll(items, MAX_QUERY_RESULTS);
    int shown = 0;

    printf("\n%-5s %-24s %-12s %-8s %-8s\n", "ID", "Name", "Batch", "Qty", "Reorder");
    printf("--------------------------------------------------------\n");
    for (int i = 0; i < count; i++) {
        if (items[i]->quantity <= items[i]->reorderLevel) {
            printf("%-5d %-24s %-12s %-8d %-8d\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->quantity, items[i]->reorderLevel);
            shown++;
        }
    }
    if (shown == 0) {
        printf("(nothing at or below reorder level)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "low stock report");
}

void reportExpiry(int daysWindow, const char *viewer)
{
    Medicine *items[MAX_QUERY_RESULTS];
    int count = inventoryGetAll(items, MAX_QUERY_RESULTS);

    char today[MAX_DATE_LEN];
    getCurrentDate(today, sizeof(today));
    int td, tm, ty;
    sscanf(today, "%2d-%2d-%4d", &td, &tm, &ty);
    long todayJDN = dateToJDN(td, tm, ty);

    printf("\n-- Already expired --\n");
    int expiredShown = 0;
    for (int i = 0; i < count; i++) {
        int d, m, y;
        sscanf(items[i]->expiryDate, "%2d-%2d-%4d", &d, &m, &y);
        long diff = dateToJDN(d, m, y) - todayJDN;
        if (diff < 0) {
            printf("  [%d] %s batch %s expired on %s (%ld day(s) ago)\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->expiryDate, -diff);
            expiredShown++;
        }
    }
    if (expiredShown == 0) {
        printf("  (none)\n");
    }

    printf("\n-- Expiring within %d day(s) --\n", daysWindow);
    int soonShown = 0;
    for (int i = 0; i < count; i++) {
        int d, m, y;
        sscanf(items[i]->expiryDate, "%2d-%2d-%4d", &d, &m, &y);
        long diff = dateToJDN(d, m, y) - todayJDN;
        if (diff >= 0 && diff <= daysWindow) {
            printf("  [%d] %s batch %s expires %s (in %ld day(s))\n",
                   items[i]->id, items[i]->name, items[i]->batch,
                   items[i]->expiryDate, diff);
            soonShown++;
        }
    }
    if (soonShown == 0) {
        printf("  (none)\n");
    }

    char detail[MAX_LINE_LEN];
    snprintf(detail, sizeof(detail), "expiry report window=%d days", daysWindow);
    logEvent(viewer, "VIEW_REPORT", detail);
}

void reportSupplyHistory(const char *viewer)
{
    FILE *fp = fopen(SUPPLY_FILE, "r");
    printf("\n%-5s %-24s %-8s %-16s %-12s %-20s\n",
           "ID", "Medicine", "Qty", "Supplier", "Admin", "Timestamp");
    printf("---------------------------------------------------------------------------------\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            int id, qty;
            char name[MAX_NAME_LEN], supplier[MAX_SUPPLIER_LEN];
            char admin[MAX_USERNAME_LEN], ts[MAX_TIMESTAMP_LEN];

            if (sscanf(line, "%d|%63[^|]|%d|%63[^|]|%31[^|]|%19[^\n]",
                       &id, name, &qty, supplier, admin, ts) == 6) {
                printf("%-5d %-24s %-8d %-16s %-12s %-20s\n", id, name, qty, supplier, admin, ts);
                shown++;
            }
        }
        fclose(fp);
        if (shown == 0) {
            printf("(no supply history)\n");
        }
    } else {
        printf("(no supply history)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "supply history");
}

void reportDistributionHistory(const char *viewer)
{
    static const char *statusNames[] = { "FULFILLED", "PARTIAL", "REJECTED" };

    FILE *fp = fopen(REQUESTS_FILE, "r");
    printf("\n%-5s %-24s %-10s %-10s %-10s %-20s\n",
           "ID", "Medicine", "Requested", "Fulfilled", "Status", "Timestamp");
    printf("---------------------------------------------------------------------------------\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = '\0';
            int id, reqQty, fulQty, status;
            char name[MAX_NAME_LEN], ts[MAX_TIMESTAMP_LEN];

            if (sscanf(line, "%d|%63[^|]|%d|%d|%d|%19[^\n]",
                       &id, name, &reqQty, &fulQty, &status, ts) == 6) {
                const char *statusText = (status >= 0 && status <= 2) ? statusNames[status] : "?";
                printf("%-5d %-24s %-10d %-10d %-10s %-20s\n",
                       id, name, reqQty, fulQty, statusText, ts);
                shown++;
            }
        }
        fclose(fp);
        if (shown == 0) {
            printf("(no distribution history)\n");
        }
    } else {
        printf("(no distribution history)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "distribution history");
}

void reportAccountability(const char *viewer)
{
    FILE *fp = fopen(AUDIT_LOG, "r");
    printf("\n-- Accountability / Audit Trail --\n");

    if (fp) {
        char line[MAX_LINE_LEN];
        int shown = 0;
        while (fgets(line, sizeof(line), fp)) {
            fputs(line, stdout);
            shown++;
        }
        fclose(fp);
        if (shown == 0) {
            printf("(audit log is empty)\n");
        }
    } else {
        printf("(audit log is empty)\n");
    }

    logEvent(viewer, "VIEW_REPORT", "accountability report");
}
