#include <stdio.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "auth.h"
#include "inventory.h"
#include "supply.h"
#include "distribution.h"
#include "reports.h"

static void printStatus(OpStatus status)
{
    switch (status) {
        case OP_SUCCESS:            printf("Done.\n"); break;
        case OP_NOT_FOUND:          printf("Not found.\n"); break;
        case OP_DUPLICATE:          printf("That already exists.\n"); break;
        case OP_INVALID_INPUT:      printf("Invalid input - nothing was changed.\n"); break;
        case OP_INSUFFICIENT_STOCK: printf("Not enough stock for that operation.\n"); break;
        case OP_FILE_ERROR:         printf("A file error occurred - nothing was changed.\n"); break;
    }
}

static void clearInputLine(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard */
    }
}

static int readMenuChoice(void)
{
    int choice;
    if (scanf("%d", &choice) != 1) {
        clearInputLine();
        return -1;
    }
    clearInputLine();
    return choice;
}

static void doLogin(User *sessionUser, int *loggedIn)
{
    char username[MAX_USERNAME_LEN];
    char password[MAX_PASSWORD_LEN];

    printf("Username: ");
    if (!fgets(username, sizeof(username), stdin)) {
        username[0] = '\0';
    } else {
        username[strcspn(username, "\n")] = '\0';
    }

    printf("Password: ");
    if (!fgets(password, sizeof(password), stdin)) {
        password[0] = '\0';
    } else {
        password[strcspn(password, "\n")] = '\0';
    }

    User result;
    OpStatus status = authLogin(username, password, &result);

    if (status == OP_SUCCESS) {
        *sessionUser = result;
        *loggedIn = 1;
        printf("Welcome, %s (%s).\n", result.username, result.isAdmin ? "admin" : "staff");
        return;
    }

    if (status == OP_NOT_FOUND) {
        printf("No such user.\n");
    } else if (result.locked) {
        printf("Account is locked. Contact an administrator.\n");
    } else {
        printf("Invalid password. Attempts used: %d/%d\n", result.failedAttempts, MAX_LOGIN_ATTEMPTS);
    }
    *loggedIn = 0;
}

static void handleAddInventory(const char *actor)
{
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN], expiryRaw[64];
    int quantity, reorderLevel;

    printf("Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), stdin)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    printf("Batch number: ");
    if (!fgets(batch, sizeof(batch), stdin)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    printf("Quantity: ");
    if (scanf("%d", &quantity) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    printf("Expiry date (D-M-YYYY): ");
    if (!fgets(expiryRaw, sizeof(expiryRaw), stdin)) {
        expiryRaw[0] = '\0';
    } else {
        expiryRaw[strcspn(expiryRaw, "\n")] = '\0';
    }

    printf("Reorder level: ");
    if (scanf("%d", &reorderLevel) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    char normalized[MAX_DATE_LEN];
    if (!normalizeDate(expiryRaw, normalized) || !isValidDate(normalized)) {
        printf("Invalid expiry date - nothing was changed.\n");
        return;
    }

    int newId;
    OpStatus status = inventoryAddNew(name, batch, quantity, normalized, reorderLevel, actor, &newId);
    if (status == OP_SUCCESS) {
        printf("Added as record id %d.\n", newId);
    } else {
        printStatus(status);
    }
}

static void handleRemoveInventory(const char *actor)
{
    int id;
    printf("Record id to remove: ");
    if (scanf("%d", &id) != 1) {
        clearInputLine();
        printf("Invalid input.\n");
        return;
    }
    clearInputLine();

    printStatus(inventoryRemove(id, actor));
}

static void handleUpdateInventory(const char *actor)
{
    int id, reorderLevel;
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN];

    printf("Record id to update: ");
    if (scanf("%d", &id) != 1) {
        clearInputLine();
        printf("Invalid input.\n");
        return;
    }
    clearInputLine();

    printf("New name: ");
    if (!fgets(name, sizeof(name), stdin)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    printf("New batch: ");
    if (!fgets(batch, sizeof(batch), stdin)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    printf("New reorder level: ");
    if (scanf("%d", &reorderLevel) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    printStatus(inventoryUpdate(id, name, batch, reorderLevel, actor));
}

static void handleRecordSupply(const char *actor)
{
    char name[MAX_NAME_LEN], batch[MAX_BATCH_LEN], expiryRaw[64], supplier[MAX_SUPPLIER_LEN];
    int quantity, reorderLevel;

    printf("Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), stdin)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    printf("Batch number: ");
    if (!fgets(batch, sizeof(batch), stdin)) {
        batch[0] = '\0';
    } else {
        batch[strcspn(batch, "\n")] = '\0';
    }

    printf("Quantity received: ");
    if (scanf("%d", &quantity) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    printf("Expiry date (D-M-YYYY): ");
    if (!fgets(expiryRaw, sizeof(expiryRaw), stdin)) {
        expiryRaw[0] = '\0';
    } else {
        expiryRaw[strcspn(expiryRaw, "\n")] = '\0';
    }

    printf("Reorder level (used only if this is a new batch): ");
    if (scanf("%d", &reorderLevel) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    printf("Supplier name: ");
    if (!fgets(supplier, sizeof(supplier), stdin)) {
        supplier[0] = '\0';
    } else {
        supplier[strcspn(supplier, "\n")] = '\0';
    }

    OpStatus status = supplyProcess(name, batch, quantity, expiryRaw, reorderLevel, supplier, actor);
    printStatus(status);
}

static void handleDistributionRequest(const char *actor)
{
    char name[MAX_NAME_LEN];
    int quantity;

    printf("Medicine/equipment name: ");
    if (!fgets(name, sizeof(name), stdin)) {
        name[0] = '\0';
    } else {
        name[strcspn(name, "\n")] = '\0';
    }

    printf("Quantity requested: ");
    if (scanf("%d", &quantity) != 1) {
        clearInputLine();
        printf("Invalid input.\n");
        return;
    }
    clearInputLine();

    SupplyRequest result;
    OpStatus status = distributionProcessRequest(name, quantity, actor, &result);

    if (status != OP_SUCCESS) {
        printStatus(status);
        return;
    }

    const char *statusText = (result.status == REQ_FULFILLED) ? "FULFILLED" :
                              (result.status == REQ_PARTIAL) ? "PARTIAL" : "REJECTED";
    printf("Request #%d: %s (%d of %d fulfilled)\n",
           result.requestId, statusText, result.fulfilledQty, result.requestedQty);
}

static void handleExpiryReport(const char *actor)
{
    int days;
    printf("Show items expiring within how many days? ");
    if (scanf("%d", &days) != 1 || days < 0) {
        clearInputLine();
        printf("Invalid input.\n");
        return;
    }
    clearInputLine();
    reportExpiry(days, actor);
}

static void handleCreateUser(const char *actor)
{
    char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
    int roleChoice;

    printf("New username: ");
    if (!fgets(username, sizeof(username), stdin)) {
        username[0] = '\0';
    } else {
        username[strcspn(username, "\n")] = '\0';
    }

    printf("New password: ");
    if (!fgets(password, sizeof(password), stdin)) {
        password[0] = '\0';
    } else {
        password[strcspn(password, "\n")] = '\0';
    }

    printf("Role (1 = admin, 2 = staff): ");
    if (scanf("%d", &roleChoice) != 1) {
        clearInputLine();
        printf("Invalid input - nothing was changed.\n");
        return;
    }
    clearInputLine();

    if (roleChoice != 1 && roleChoice != 2) {
        printf("Invalid role - nothing was changed.\n");
        return;
    }

    printStatus(authCreateUser(username, password, roleChoice == 1, actor));
}

static void printMenu(int isAdmin)
{
    printf("\n===== Medical Supply Management System =====\n");
    printf(" 1. View stock report\n");
    printf(" 2. View low stock report\n");
    printf(" 3. View expiry report\n");
    printf(" 4. View supply history\n");
    printf(" 5. View distribution history\n");
    printf(" 6. Process a distribution request\n");
    if (isAdmin) {
        printf(" 7. Add inventory item\n");
        printf(" 8. Remove inventory item\n");
        printf(" 9. Update inventory item\n");
        printf("10. Record supply received\n");
        printf("11. Create new user\n");
        printf("12. View accountability report\n");
    }
    printf(" 0. Logout\n");
    printf("Choice: ");
}

/* runs the post-login menu for one user until they log out.
 * returns 1 if the user wants to log in again as someone else,
 * 0 if they want to exit the program. */
static int runSession(const User *sessionUser)
{
    int sessionActive = 1;
    while (sessionActive) {
        printMenu(sessionUser->isAdmin);
        int choice = readMenuChoice();

        switch (choice) {
            case 1: reportStock(sessionUser->username); break;
            case 2: reportLowStock(sessionUser->username); break;
            case 3: handleExpiryReport(sessionUser->username); break;
            case 4: reportSupplyHistory(sessionUser->username); break;
            case 5: reportDistributionHistory(sessionUser->username); break;
            case 6: handleDistributionRequest(sessionUser->username); break;
            case 7:
                if (sessionUser->isAdmin) handleAddInventory(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 8:
                if (sessionUser->isAdmin) handleRemoveInventory(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 9:
                if (sessionUser->isAdmin) handleUpdateInventory(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 10:
                if (sessionUser->isAdmin) handleRecordSupply(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 11:
                if (sessionUser->isAdmin) handleCreateUser(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 12:
                if (sessionUser->isAdmin) reportAccountability(sessionUser->username);
                else printf("Admins only.\n");
                break;
            case 0:
                sessionActive = 0;
                printf("Logged out.\n");
                break;
            default:
                printf("Unknown option.\n");
                break;
        }
    }

    printf("Log in as another user? (1 = yes, 0 = exit program): ");
    return (readMenuChoice() == 1);
}

int main(void)
{
    ensureDataDir();
    inventoryInit();
    authInit();

    int running = 1;
    while (running) {
        User sessionUser;
        int loggedIn = 0;

        printf("\n--- Login ---\n");
        while (!loggedIn) {
            doLogin(&sessionUser, &loggedIn);
            if (!loggedIn) {
                printf("Try again? (1 = yes, 0 = exit program): ");
                int retry = readMenuChoice();
                if (retry != 1) {
                    running = 0;
                    break;
                }
            }
        }
        if (!loggedIn) {
            break;
        }

        running = runSession(&sessionUser);
    }

    inventoryFreeAll();
    authFreeAll();
    printf("Goodbye.\n");
    return 0;
}
