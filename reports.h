#ifndef REPORTS_H
#define REPORTS_H

void reportStock(const char *viewer);
void reportLowStock(const char *viewer);
void reportExpiry(int daysWindow, const char *viewer);
void reportSupplyHistory(const char *viewer);
void reportDistributionHistory(const char *viewer);
void reportAccountability(const char *viewer);

#endif
