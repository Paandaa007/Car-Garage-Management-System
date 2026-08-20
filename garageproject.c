#include <stdio.h> 
#include <string.h> 
#include <stdlib.h> 

#define MAX_VEHICLES 100 
#define MAX_PARTS 10 
#define MAX_CUSTOMERS 50
#define MAX_REQUESTS 20

#define PASSWORD_FILE "password.txt" 
#define TECH_PASSWORD_FILE "tech_password.txt"
#define CUSTOMER_DB_FILE "customer_db.txt"
#define DATA_FILE "garage_data.txt" 
#define INVENTORY_FILE "inventory_data.txt"
#define REQUESTS_FILE "requests_data.txt"

// MD5 ENGINE
void getMD5String(const char *input, char *outputString) {
    unsigned long hash = 5381;
    int c;
    while ((c = *input++)) {
        hash = ((hash << 5) + hash) + c;
    }
    sprintf(outputString, "%016lx%016lx", hash, hash ^ 0xDEADBEEF);
}

// Manager/Tech Default Security Recovery Defaults
char securityQuestion[100] = "WhatIsYourFavColor"; 
char securityAnswer[50] = "Blue"; 

// Structure for Customer User Accounts
struct CustomerAccount {
    char phone[20];
    char name[50];
    char passwordHash[33];
    char secQuestion[100];
    char secAnswer[50];
};

struct CustomerAccount customerDB[MAX_CUSTOMERS];
int customerCount = 0;
int loggedInCustomerIndex = -1;

// Structure for Inventory
struct InventoryItem { 
    char partName[30]; 
    int quantity; 
    float buyingPrice;  // INTERNAL COST (Dealer Price - Manager Only)
    float sellingPrice; // CUSTOMER PRICE (Manager Configured)
}; 

// Structure for Technician Part Requests to Manager (No Price Input by Tech)
struct PartRequest {
    int requestId;
    char techName[30];
    char partName[30];
    int quantityRequested;
    int status; // 0 = Pending Manager, 1 = Approved & Restocked from Dealer, 2 = Rejected
};

struct PartRequest partRequests[MAX_REQUESTS];
int requestCount = 0;

// Structure for Vehicle Intake
struct Vehicle { 
    int id; 
    char ownerName[50]; 
    char ownerPhone[20];
    char serviceType[30]; 
    char extraIssues[100]; 
    char requestedPart[30]; 
    int partAvailable; 
    char feedback[150]; 
    int rating; 
    int estHours; 
    char phase[20]; 
    char notes[200]; 
    float serviceCost; 
    float partscost;
    
    // Extra Part Workflow
    char extraPartRequested[30];
    float extraPartBuyingCost;
    float extraPartSellingCost;
    int approvalStatus; // 0 = None, 1 = Pending Customer Approval, 2 = (YES), 3 =  (NO)
    
    float totalBill; 
}; 

struct Vehicle garage[MAX_VEHICLES]; 
int VehicleCount = 0; 

// Initial Inventory Fallback Items
struct InventoryItem inventory[MAX_PARTS] = { 
    {"Mirror", 4, 15.00, 30.00}, 
    {"Tire", 6, 45.00, 80.00}, 
    {"Brake Pad", 2, 60.00, 120.00}, 
    {"Spark Plug", 10, 10.00, 25.00}, 
    {"Battery", 3, 70.00, 130.00} 
}; 
int inventoryCount = 5;

// Financial Metrics 
float totalDailySales = 0.0; 
float totalMonthlySales = 0.0; 
const float EMPLOYEE_SALARY_COST = 10000.00; 

// Function Prototypes
void forgotPassword(int role); 
void saveCustomerAccounts();
void loadCustomerAccounts();
void saveInventoryData();
void loadInventoryData();
void savePartRequestsData();
void loadPartRequestsData();

void checkFirstTime() { 
    FILE *file = fopen(PASSWORD_FILE, "r"); 
    if (file == NULL) { 
        printf("----- Welcome to Garage Service System Setup -----\n"); 
        printf("First-time setup detected. Configuring initial passwords.\n");
        
        char newPassword[50], hashedPass[33]; 
        printf("Set Manager/Admin password: "); 
        scanf("%s", newPassword); 
        
        getMD5String(newPassword, hashedPass);
        
        file = fopen(PASSWORD_FILE, "w"); 
        if (file != NULL) {
            fprintf(file, "%s", hashedPass); 
            fclose(file); 
        }

        FILE *techFile = fopen(TECH_PASSWORD_FILE, "w");
        if (techFile != NULL) {
            char techHash[33];
            getMD5String("tech123", techHash);
            fprintf(techFile, "%s", techHash);
            fclose(techFile);
        }

        printf("System accounts configured! Default Tech Password: 'tech123'\nRestarting...\n"); 
        exit(0); 
    } 
    fclose(file); 
} 

// INVENTORY FILE PERSISTENCE
void saveInventoryData() {
    FILE *file = fopen(INVENTORY_FILE, "w");
    if (file == NULL) return;
    for (int i = 0; i < inventoryCount; i++) {
        fprintf(file, "%s|%d|%.2f|%.2f\n", 
                inventory[i].partName, inventory[i].quantity, 
                inventory[i].buyingPrice, inventory[i].sellingPrice);
    }
    fclose(file);
}

void loadInventoryData() {
    FILE *file = fopen(INVENTORY_FILE, "r");
    if (file == NULL) return; 
    inventoryCount = 0;
    char line[200];
    while (fgets(line, sizeof(line), file)) {
        struct InventoryItem item;
        char *token = strtok(line, "|");
        if (token) strcpy(item.partName, token);
        token = strtok(NULL, "|");
        if (token) item.quantity = atoi(token);
        token = strtok(NULL, "|");
        if (token) item.buyingPrice = atof(token);
        token = strtok(NULL, "\n");
        if (token) item.sellingPrice = atof(token);
        
        inventory[inventoryCount++] = item;
    }
    fclose(file);
}

// PART REQUESTS FILE PERSISTENCE (NO PRICES SAVED BY TECH)
void savePartRequestsData() {
    FILE *file = fopen(REQUESTS_FILE, "w");
    if (file == NULL) return;
    for (int i = 0; i < requestCount; i++) {
        fprintf(file, "%d|%s|%s|%d|%d\n",
                partRequests[i].requestId, partRequests[i].techName,
                partRequests[i].partName, partRequests[i].quantityRequested,
                partRequests[i].status);
    }
    fclose(file);
}

void loadPartRequestsData() {
    FILE *file = fopen(REQUESTS_FILE, "r");
    if (file == NULL) return;
    requestCount = 0;
    char line[200];
    while (fgets(line, sizeof(line), file)) {
        struct PartRequest req;
        char *token = strtok(line, "|");
        if (token) req.requestId = atoi(token);
        token = strtok(NULL, "|");
        if (token) strcpy(req.techName, token);
        token = strtok(NULL, "|");
        if (token) strcpy(req.partName, token);
        token = strtok(NULL, "|");
        if (token) req.quantityRequested = atoi(token);
        token = strtok(NULL, "\n");
        if (token) req.status = atoi(token);

        partRequests[requestCount++] = req;
    }
    fclose(file);
}

void registerCustomer() {
    if (customerCount >= MAX_CUSTOMERS) {
        printf("Customer database limit reached.\n");
        return;
    }

    struct CustomerAccount newCust;
    printf("\n------ CUSTOMER REGISTRATION ------\n");
    printf("Enter Phone Number (Used as User ID): ");
    scanf("%s", newCust.phone);

    for (int i = 0; i < customerCount; i++) {
        if (strcmp(customerDB[i].phone, newCust.phone) == 0) {
            printf("An account with this phone number already exists!\n");
            return;
        }
    }

    while (getchar() != '\n');
    printf("Enter Full Name: ");
    fgets(newCust.name, 50, stdin);
    newCust.name[strcspn(newCust.name, "\n")] = 0;

    char pass[50], passConfirm[50];
    printf("Create Password: ");
    scanf("%s", pass);
    printf("Confirm Password: ");
    scanf("%s", passConfirm);

    if (strcmp(pass, passConfirm) != 0) {
        printf("Passwords do not match. Registration failed.\n");
        return;
    }
    getMD5String(pass, newCust.passwordHash);

    while (getchar() != '\n');
    printf("Set a Security Question: ");
    fgets(newCust.secQuestion, 100, stdin);
    newCust.secQuestion[strcspn(newCust.secQuestion, "\n")] = 0;

    printf("Enter Security Answer: ");
    fgets(newCust.secAnswer, 50, stdin);
    newCust.secAnswer[strcspn(newCust.secAnswer, "\n")] = 0;

    customerDB[customerCount++] = newCust;
    saveCustomerAccounts();
    printf("Registration Successful! You can now log in.\n");
}

void saveCustomerAccounts() {
    FILE *file = fopen(CUSTOMER_DB_FILE, "w");
    if (file == NULL) return;
    for (int i = 0; i < customerCount; i++) {
        fprintf(file, "%s|%s|%s|%s|%s\n", 
                customerDB[i].phone, customerDB[i].name, 
                customerDB[i].passwordHash, customerDB[i].secQuestion, customerDB[i].secAnswer);
    }
    fclose(file);
}

void loadCustomerAccounts() {
    FILE *file = fopen(CUSTOMER_DB_FILE, "r");
    if (file == NULL) return;
    customerCount = 0;
    char line[300];
    while (fgets(line, sizeof(line), file)) {
        struct CustomerAccount c;
        char *token = strtok(line, "|");
        if (token) strcpy(c.phone, token);
        token = strtok(NULL, "|");
        if (token) strcpy(c.name, token);
        token = strtok(NULL, "|");
        if (token) strcpy(c.passwordHash, token);
        token = strtok(NULL, "|");
        if (token) strcpy(c.secQuestion, token);
        token = strtok(NULL, "\n");
        if (token) strcpy(c.secAnswer, token);
        
        customerDB[customerCount++] = c;
    }
    fclose(file);
}

void forgotPasswordCustomer() {
    char phone[20], answer[50], newPass[50], confirmPass[50];
    printf("\n------ CUSTOMER PASSWORD RECOVERY ------\n");
    printf("Enter Registered Phone Number: ");
    scanf("%s", phone);

    int idx = -1;
    for (int i = 0; i < customerCount; i++) {
        if (strcmp(customerDB[i].phone, phone) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        printf("Phone number not registered.\n");
        return;
    }

    while (getchar() != '\n');
    printf("Security Question: %s\nYour Answer: ", customerDB[idx].secQuestion);
    fgets(answer, 50, stdin);
    answer[strcspn(answer, "\n")] = 0;

    if (strcasecmp(answer, customerDB[idx].secAnswer) == 0) {
        printf("Verification Successful! Enter New Password: ");
        scanf("%s", newPass);
        printf("Confirm New Password: ");
        scanf("%s", confirmPass);

        if (strcmp(newPass, confirmPass) == 0) {
            getMD5String(newPass, customerDB[idx].passwordHash);
            saveCustomerAccounts();
            printf("Password recovered successfully!\n");
        } else {
            printf("Passwords do not match.\n");
        }
    } else {
        printf("Incorrect Answer!\n");
    }
}

int LoginUser(int role) { 
    char enteredPassword[50], enteredHash[33], savedPasswordHash[50]; 
    int choice; 

    if (role == 3) {
        printf("\n------ CUSTOMER PORTAL ------\n");
        printf("1. Login\n2. Register New Account\n3. Forgot Password\nSelect option: ");
        scanf("%d", &choice);

        if (choice == 2) {
            registerCustomer();
            return 0;
        } else if (choice == 3) {
            forgotPasswordCustomer();
            return 0;
        } else if (choice != 1) {
            return 0;
        }

        char phone[20];
        printf("Enter Phone Number: ");
        scanf("%s", phone);
        printf("Enter Password: ");
        scanf("%s", enteredPassword);

        getMD5String(enteredPassword, enteredHash);

        for (int i = 0; i < customerCount; i++) {
            if (strcmp(customerDB[i].phone, phone) == 0 && strcmp(customerDB[i].passwordHash, enteredHash) == 0) {
                loggedInCustomerIndex = i;
                printf("Welcome back, %s!\n", customerDB[i].name);
                return 1;
            }
        }
        printf("Invalid Phone Number or Password.\n");
        return 0;
    }

    const char *passFile = (role == 1) ? PASSWORD_FILE : TECH_PASSWORD_FILE;
    FILE *file = fopen(passFile, "r"); 
    if (file == NULL) {
        getMD5String(role == 1 ? "admin" : "tech123", savedPasswordHash);
    } else {
        fscanf(file, "%s", savedPasswordHash); 
        fclose(file); 
    }

    printf("\n------ %s Login ------\n", role == 1 ? "Manager/Admin" : "Technician");
    printf("1. Enter Password\n2. Forgot Password\nSelect option: "); 
    if (scanf("%d", &choice) != 1) { 
        while (getchar() != '\n'); 
        return 0; 
    } 

    if (choice == 2) { 
        forgotPassword(role); 
        return 0; 
    } else if (choice != 1) {
        return 0;
    }

    printf("Enter Password: "); 
    scanf("%s", enteredPassword); 
    getMD5String(enteredPassword, enteredHash);
    
    if (strcmp(enteredHash, savedPasswordHash) == 0) { 
        printf("Authentication successful!\n"); 
        return 1; 
    } else { 
        printf("Incorrect password.\n"); 
        return 0; 
    } 
} 

void resetPassword(int role) { 
    if (role == 3) {
        if (loggedInCustomerIndex == -1) return;
        char newPass[50], confirmPass[50];
        printf("Enter New Password: ");
        scanf("%s", newPass);
        printf("Confirm New Password: ");
        scanf("%s", confirmPass);
        if (strcmp(newPass, confirmPass) == 0) {
            getMD5String(newPass, customerDB[loggedInCustomerIndex].passwordHash);
            saveCustomerAccounts();
            printf("Password successfully updated!\n");
        } else {
            printf("Passwords do not match.\n");
        }
        return;
    }

    char currentPassword[50], currentHash[33], savedPasswordHash[50], newPassword[50], confirmPassword[50], newHash[33]; 
    const char *passFile = (role == 1) ? PASSWORD_FILE : TECH_PASSWORD_FILE;

    FILE *file = fopen(passFile, "r"); 
    if (file == NULL) return; 
    fscanf(file, "%s", savedPasswordHash); 
    fclose(file); 

    printf("Enter current password: "); 
    scanf("%s", currentPassword); 
    getMD5String(currentPassword, currentHash);
    
    if (strcmp(currentHash, savedPasswordHash) == 0) { 
        printf("Enter new password: "); 
        scanf("%s", newPassword); 
        printf("Confirm new password: "); 
        scanf("%s", confirmPassword); 
        if (strcmp(newPassword, confirmPassword) == 0) { 
            getMD5String(newPassword, newHash);
            file = fopen(passFile, "w"); 
            if (file != NULL) { 
                fprintf(file, "%s", newHash); 
                fclose(file); 
                printf("Password reset successful!\n"); 
            } 
        } else { 
            printf("Passwords do not match.\n"); 
        } 
    } else { 
        printf("Incorrect current password.\n"); 
    } 
} 

void forgotPassword(int role) { 
    char enteredAnswer[50], newPassword[50], confirmPassword[50], newHash[33]; 
    const char *passFile = (role == 1) ? PASSWORD_FILE : TECH_PASSWORD_FILE;

    printf("\n------ %s Password Recovery ------\n", role == 1 ? "Manager/Admin" : "Technician"); 
    printf("Security Question: %s\nYour Answer: ", securityQuestion); 
    scanf("%s", enteredAnswer); 

    if (strcmp(enteredAnswer, securityAnswer) == 0) { 
        printf("Verification successful!\nEnter new password: "); 
        scanf("%s", newPassword); 
        printf("Confirm new password: "); 
        scanf("%s", confirmPassword); 

        if (strcmp(newPassword, confirmPassword) == 0) { 
            getMD5String(newPassword, newHash);
            FILE *file = fopen(passFile, "w"); 
            if (file != NULL) { 
                fprintf(file, "%s", newHash); 
                fclose(file); 
                printf("Password reset successfully.\n"); 
            } 
        } else { 
            printf("Passwords do not match.\n"); 
        } 
    } else { 
        printf("Incorrect Answer.\n"); 
    } 
} 

int isIdDuplicate(int id) { 
    for (int i = 0; i < VehicleCount; i++) { 
        if (garage[i].id == id) return 1; 
    } 
    return 0; 
} 

void logNewVehicle() { 
    if (VehicleCount >= MAX_VEHICLES) { 
        printf("Database storage limits exhausted!\n"); 
        return; 
    } 

    int id; 
    printf("Enter Vehicle ID (or 0 to cancel): "); 
    scanf("%d", &id); 
    if (id == 0) return; 

    if (isIdDuplicate(id)) { 
        printf("Duplicate Vehicle ID detected!\n"); 
        return; 
    } 

    struct Vehicle *v = &garage[VehicleCount]; 
    v->id = id; 
    while (getchar() != '\n'); 
    printf("Enter Owner Name: "); 
    fgets(v->ownerName, 50, stdin); 
    v->ownerName[strcspn(v->ownerName, "\n")] = 0; 

    printf("Enter Owner Registered Phone Number: ");
    scanf("%s", v->ownerPhone);

    int serviceChoice = 0; 
    v->serviceCost = 0.0; 
    printf("\nSelect Service Type:\n1. Basic Checkup ($50.00)\n2. Deep Service ($150.00)\n3. Electrical Diagnosis ($200.00)\nChoice: "); 
    scanf("%d", &serviceChoice); 
    if (serviceChoice == 1) { 
        strcpy(v->serviceType, "Basic Checkup"); 
        v->serviceCost = 50.00; 
    } else if (serviceChoice == 2) { 
        strcpy(v->serviceType, "Deep Service"); 
        v->serviceCost = 150.00; 
    } else { 
        strcpy(v->serviceType, "Electrical Diagnosis"); 
        v->serviceCost = 200.00; 
    } 

    v->partAvailable = 1; 
    v->partscost = 0.0; 
    strcpy(v->requestedPart, "None"); 
    strcpy(v->extraPartRequested, "None");
    v->extraPartBuyingCost = 0.0;
    v->extraPartSellingCost = 0.0;
    v->approvalStatus = 0;

    printf("Enter Estimated Labor Hours: "); 
    scanf("%d", &v->estHours); 

    strcpy(v->phase, "Inspection"); 
    strcpy(v->notes, "Vehicle registered. Awaiting technician inspection."); 

    v->totalBill = v->serviceCost + (v->estHours * 70.0); 
    totalDailySales += v->totalBill; 
    totalMonthlySales += v->totalBill; 

    VehicleCount++; 
    printf("\nVehicle logged successfully!\n"); 
} 

void viewActiveJobs() { 
    printf("\nID     Owner           Phone         Service Type         Phase          Approval Status       Invoice\n"); 
    printf("---------------------------------------------------------------------------------------------------------\n"); 
    for (int i = 0; i < VehicleCount; i++) { 
        char appStr[30] = "N/A";
        if (garage[i].approvalStatus == 1) strcpy(appStr, "Pending Customer YES/NO");
        else if (garage[i].approvalStatus == 2) strcpy(appStr, "APPROVED (YES)");
        else if (garage[i].approvalStatus == 3) strcpy(appStr, "REJECTED (NO)");

        printf("%-6d %-15s %-13s %-20s %-14s %-22s $%.2f\n", 
               garage[i].id, garage[i].ownerName, garage[i].ownerPhone, garage[i].serviceType, garage[i].phase, appStr, garage[i].totalBill); 
    } 
    printf("---------------------------------------------------------------------------------------------------------\n"); 
} 

// TECHNICIAN INVENTORY VIEW (STOCK ONLY - NO DEALER PRICING SHOWN)
void viewInventoryTech() {
    printf("\n%-4s %-20s %-12s\n", "No.", "Part Name", "Stock Level");
    printf("------------------------------------\n");
    for (int i = 0; i < inventoryCount; i++) {
        printf("%-4d %-20s %-12d\n", i + 1, inventory[i].partName, inventory[i].quantity);
    }
}

// TECHNICIAN PART REQUEST TO MANAGER (NO PRICING INPUT)
void technicianRequestPartFromManager() {
    if (requestCount >= MAX_REQUESTS) {
        printf("Part request queue is full.\n");
        return;
    }

    struct PartRequest *req = &partRequests[requestCount];
    req->requestId = requestCount + 1;

    printf("\n--- REQUEST OUT-OF-STOCK PART FROM MANAGER ---\n");
    printf("Enter Tech Name: ");
    scanf("%s", req->techName);
    printf("Enter Missing Part Name Needed: ");
    scanf("%s", req->partName);
    printf("Enter Quantity Needed: ");
    scanf("%d", &req->quantityRequested);

    req->status = 0; // Pending Manager Review
    requestCount++;
    savePartRequestsData();
    printf("Part request #%d for %d x '%s' sent to Manager for dealer procurement!\n", 
           req->requestId, req->quantityRequested, req->partName);
}

void managerInventoryAndDealerOrders() {
    while (1) {
        printf("\n====== MANAGER MAIN INVENTORY & DEALER PORTAL ======\n");
        printf("1. View Full Inventory Stock & Pricing\n");
        printf("2. Review Technician Out-of-Stock Requests & Order from Dealer\n");
        printf("3. Manually Add New Part / Direct Restock from Dealer\n");
        printf("0. Back to Main Admin Menu\n");
        printf("Choice: ");
        int choice;
        scanf("%d", &choice);

        if (choice == 1) {
            printf("\n%-4s %-15s %-10s %-15s %-15s\n", "No.", "Part Name", "Stock", "Buying Price", "Selling Price");
            printf("-------------------------------------------------------------\n");
            for (int i = 0; i < inventoryCount; i++) {
                printf("%-4d %-15s %-10d $%-14.2f $%-14.2f\n", 
                       i + 1, inventory[i].partName, inventory[i].quantity, inventory[i].buyingPrice, inventory[i].sellingPrice);
            }
        } else if (choice == 2) {
            printf("\n--- PENDING TECHNICIAN PART REQUESTS ---\n");
            int pending = 0;
            for (int i = 0; i < requestCount; i++) {
                if (partRequests[i].status == 0) {
                    pending++;
                    printf("ID: %d | Tech: %s | Requested Part: %s | Qty: %d\n",
                           partRequests[i].requestId, partRequests[i].techName, 
                           partRequests[i].partName, partRequests[i].quantityRequested);
                }
            }
            if (pending == 0) {
                printf("No pending requests from technicians.\n");
                continue;
            }

            printf("\nEnter Request ID to Approve & Order from Dealer (or 0 to cancel): ");
            int reqId;
            scanf("%d", &reqId);
            if (reqId == 0) continue;

            int found = -1;
            for (int i = 0; i < requestCount; i++) {
                if (partRequests[i].requestId == reqId && partRequests[i].status == 0) {
                    found = i;
                    break;
                }
            }

            if (found != -1) {
                printf("1. Approve & Order from Dealer\n2. Reject Request\nChoice: ");
                int dec;
                scanf("%d", &dec);
                if (dec == 1) {
                    partRequests[found].status = 1;
                    int invIdx = -1;
                    for (int j = 0; j < inventoryCount; j++) {
                        if (strcasecmp(inventory[j].partName, partRequests[found].partName) == 0) {
                            invIdx = j;
                            break;
                        }
                    }

                    if (invIdx != -1) {
                        inventory[invIdx].quantity += partRequests[found].quantityRequested;
                    } else if (inventoryCount < MAX_PARTS) {
                        strcpy(inventory[inventoryCount].partName, partRequests[found].partName);
                        inventory[inventoryCount].quantity = partRequests[found].quantityRequested;
                        
                        // Manager configures dealer buying cost and customer price when receiving stock
                        printf("Set Dealer Buying Price per unit ($): ");
                        scanf("%f", &inventory[inventoryCount].buyingPrice);
                        printf("Set Customer Selling Price per unit ($): ");
                        scanf("%f", &inventory[inventoryCount].sellingPrice);
                        
                        inventoryCount++;
                    }
                    saveInventoryData();
                    savePartRequestsData();
                    printf("Order placed to Dealer! Stock added to Inventory.\n");
                } else {
                    partRequests[found].status = 2;
                    savePartRequestsData();
                    printf("Request rejected.\n");
                }
            }
        } else if (choice == 3) {
            if (inventoryCount >= MAX_PARTS) {
                printf("Inventory capacity full.\n");
                continue;
            }
            struct InventoryItem newItem;
            printf("Enter Part Name: ");
            scanf("%s", newItem.partName);
            printf("Enter Quantity Ordered from Dealer: ");
            scanf("%d", &newItem.quantity);
            printf("Enter Dealer Buying Price ($): ");
            scanf("%f", &newItem.buyingPrice);
            printf("Enter Customer Selling Price ($): ");
            scanf("%f", &newItem.sellingPrice);

            inventory[inventoryCount++] = newItem;
            saveInventoryData();
            printf("Dealer order received! New item added to stock.\n");
        } else if (choice == 0) {
            break;
        }
    }
}

// INSPECTION & AUTOMATIC QUOTE GENERATION (TECH SELECTS PART, SYSTEM COUNTS PRICE)
void inspectAndRequestExtraParts() {
    if (VehicleCount == 0) {
        printf("No active vehicles registered.\n");
        return;
    }
    int id, foundIndex = -1;
    printf("Enter Vehicle ID to inspect: ");
    scanf("%d", &id);

    for (int i = 0; i < VehicleCount; i++) {
        if (garage[i].id == id) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex == -1) {
        printf("Vehicle ID not found.\n");
        return;
    }

    struct Vehicle *v = &garage[foundIndex];
    printf("\nInspecting Vehicle ID %d (Owner: %s)\n", v->id, v->ownerName);
    printf("Does this car require extra replacement parts?\n1. Yes (Send Quote Notification to Customer)\n2. No\nSelect: ");
    int choice;
    scanf("%d", &choice);

    if (choice == 1) {
        printf("\n--- CURRENT INVENTORY STOCK ---\n");
        viewInventoryTech();
        printf("Select part index (1-%d): ", inventoryCount);
        int partIdx;
        scanf("%d", &partIdx);
        partIdx--;

        if (partIdx >= 0 && partIdx < inventoryCount) {
            if (inventory[partIdx].quantity <= 0) {
                printf("Part out of stock! Please submit a stock request to the Manager first.\n");
                return;
            }

            // SYSTEM AUTOMATICALLY LOOKS UP SELLING PRICE (NO PRICE INPUT BY TECH)
            strcpy(v->extraPartRequested, inventory[partIdx].partName);
            v->extraPartBuyingCost = inventory[partIdx].buyingPrice;
            v->extraPartSellingCost = inventory[partIdx].sellingPrice;
            v->approvalStatus = 1; // Set to Pending Customer Approval

            snprintf(v->notes, sizeof(v->notes), "Extra Part Needed: %s. Automatic System Quote ($%.2f) sent to Customer.", 
                     v->extraPartRequested, v->extraPartSellingCost);
            
            printf("[NOTIFICATION SENT TO CUSTOMER PORTAL] Part: %s | Auto-Calculated Price: $%.2f\n", 
                   v->extraPartRequested, v->extraPartSellingCost);
        } else {
            printf("Invalid part selection.\n");
        }
    }
}

void customerMenu() {
    if (loggedInCustomerIndex == -1) return;
    struct CustomerAccount *cust = &customerDB[loggedInCustomerIndex];

    while (1) {
        printf("\n================ CUSTOMER PORTAL ================\n");
        printf("Logged in as: %s (Phone: %s)\n", cust->name, cust->phone);
        printf("1. View My Vehicle Status & Invoices\n");
        printf("2. Check Pending Extra Part Repair Approvals\n");
        printf("3. Reset Password\n");
        printf("0. Logout\n");
        printf("------------------------------------------------\n");
        printf("Choice: ");
        int choice;
        scanf("%d", &choice);

        if (choice == 1) {
            printf("\n--- YOUR VEHICLES IN SERVICE ---\n");
            int found = 0;
            for (int i = 0; i < VehicleCount; i++) {
                if (strcmp(garage[i].ownerPhone, cust->phone) == 0) {
                    found = 1;
                    printf("Vehicle ID: %d | Service: %s | Status Phase: %s\n", 
                           garage[i].id, garage[i].serviceType, garage[i].phase);
                    printf("Tech Notes: %s\n", garage[i].notes);
                    printf("Current Estimated Invoice: $%.2f\n\n", garage[i].totalBill);
                }
            }
            if (!found) printf("No vehicles logged under phone number %s.\n", cust->phone);

        } else if (choice == 2) {
            printf("\n--- PENDING EXTRA REPAIR QUOTES ---\n");
            int pending = 0;
            for (int i = 0; i < VehicleCount; i++) {
                if (strcmp(garage[i].ownerPhone, cust->phone) == 0 && garage[i].approvalStatus == 1) {
                    pending++;
                    struct Vehicle *v = &garage[i];
                    printf("\nNOTIFICATION FOR VEHICLE ID %d:\n", v->id);
                    printf("Technician states your car requires: %s\n", v->extraPartRequested);
                    printf("System Calculated Repair Cost: $%.2f\n", v->extraPartSellingCost);
                    printf("1. Approve (YES - Technician will perform repair)\n");
                    printf("2. Reject  (NO  - Technician will NOT perform repair)\nSelect: ");
                    int resp;
                    scanf("%d", &resp);

                    if (resp == 1) {
                        v->approvalStatus = 2; // YES: APPROVED
                        v->totalBill += v->extraPartSellingCost;
                        totalDailySales += v->extraPartSellingCost;
                        totalMonthlySales += v->extraPartSellingCost;

                        // Deduct part from stock
                        for (int k = 0; k < inventoryCount; k++) {
                            if (strcasecmp(inventory[k].partName, v->extraPartRequested) == 0 && inventory[k].quantity > 0) {
                                inventory[k].quantity--;
                                break;
                            }
                        }
                        saveInventoryData();
                        strcpy(v->phase, "In Progress");
                        snprintf(v->notes, sizeof(v->notes), "Customer APPROVED %s ($%.2f). Technician work in progress.", 
                                 v->extraPartRequested, v->extraPartSellingCost);
                        printf("You APPROVED the repair! Technician will proceed with installation.\n");
                    } else {
                        v->approvalStatus = 3; // NO: REJECTED
                        snprintf(v->notes, sizeof(v->notes), "Customer REJECTED %s. Technician skipping this extra repair.", 
                                 v->extraPartRequested);
                        printf("You REJECTED the repair. Technician will NOT work on this part.\n");
                    }
                }
            }
            if (pending == 0) printf("No pending repair notifications requiring approval.\n");

        } else if (choice == 3) {
            resetPassword(3);
        } else if (choice == 0) {
            loggedInCustomerIndex = -1;
            printf("Logged out successfully.\n");
            return;
        }
    }
}

void updateRepairStatus() { 
    if (VehicleCount == 0) return; 
    int id, foundIndex = -1; 
    printf("Enter Vehicle ID to update: "); 
    scanf("%d", &id); 

    for (int i = 0; i < VehicleCount; i++) { 
        if (garage[i].id == id) { 
            foundIndex = i; 
            break; 
        } 
    } 
    if (foundIndex == -1) {
        printf("Vehicle ID not found.\n");
        return; 
    }

    struct Vehicle *v = &garage[foundIndex];

    // CHECK IF WORK IS HALTED DUE TO REJECTION
    if (v->approvalStatus == 3) {
        printf("Notice: Customer REJECTED extra repair '%s'. Proceeding with standard service only.\n", v->extraPartRequested);
    } else if (v->approvalStatus == 1) {
        printf("Warning: Customer approval is PENDING for extra part '%s'. Waiting for customer 'YES'.\n", v->extraPartRequested);
    }

    int statusChoice; 
    printf("\nCurrent Phase: %s\n1. Inspection\n2. Diagnosis / Repair in Progress\n3. Completion\nSelect Stage: ", v->phase); 
    scanf("%d", &statusChoice); 

    if (statusChoice == 1) strcpy(v->phase, "Inspection"); 
    else if (statusChoice == 2) strcpy(v->phase, "Diagnosis/Repair"); 
    else if (statusChoice == 3) strcpy(v->phase, "Completion"); 

    while (getchar() != '\n'); 
    char tempNotes[200]; 
    printf("Enter Technical Notes: "); 
    fgets(tempNotes, 200, stdin); 
    tempNotes[strcspn(tempNotes, "\n")] = 0; 
    if (strlen(tempNotes) > 0) { 
        strcpy(v->notes, tempNotes); 
    } 
} 

void deleteVehicleRecord() { 
    if (VehicleCount == 0) return; 
    int id, foundIndex = -1; 
    printf("Enter ID of vehicle to delete: "); 
    scanf("%d", &id); 

    for (int i = 0; i < VehicleCount; i++) { 
        if (garage[i].id == id) { 
            foundIndex = i; 
            break; 
        } 
    } 

    if (foundIndex == -1) return; 

    for (int i = foundIndex; i < VehicleCount - 1; i++) { 
        garage[i] = garage[i + 1]; 
    } 
    VehicleCount--; 
    printf("Vehicle record removed.\n"); 
} 

void saveLogsToFile() { 
    FILE *file = fopen(DATA_FILE, "w"); 
    if (file == NULL) return; 

    fprintf(file, "ID Owner Name Phone Service Total Bill\n"); 
    fprintf(file, "--------------------------------------------------------\n"); 
    for (int i = 0; i < VehicleCount; i++) { 
        fprintf(file, "%-6d %-15s %-12s %-15s $%.2f\n", 
                garage[i].id, garage[i].ownerName, garage[i].ownerPhone, garage[i].serviceType, garage[i].totalBill); 
    } 
    fclose(file); 
    printf("System data successfully saved to file.\n"); 
} 

void viewFinancialReport() { 
    printf("\n====== EXECUTIVE FINANCIAL REPORT ======\n"); 
    printf("Daily Sales Volume     : $%10.2f\n", totalDailySales); 
    printf("Monthly Gross Sales   : $%10.2f\n", totalMonthlySales); 
    printf("-----------------------------------------\n"); 
    printf("Fixed Employee Costs  : -$%10.2f\n", EMPLOYEE_SALARY_COST); 
    
    float netProfit = totalMonthlySales - EMPLOYEE_SALARY_COST; 
    printf("Net Monthly Profit    : $%10.2f\n", netProfit); 
    printf("=========================================\n"); 
} 

void technicianMenu() {
    int choice;
    while (1) {
        printf("\n---- TECHNICIAN PORTAL ----\n");
        printf(" 1. View Active Jobs & Repair Status\n");
        printf(" 2. View Inventory Stock Levels\n");
        printf(" 3. Inspect Vehicle & Send Auto-Priced Extra Part Quote\n");
        printf(" 4. Request Out-of-Stock Part from Manager\n");
        printf(" 5. Update Vehicle Repair Phase & Technical Notes\n");
        printf(" 6. Reset Password\n");
        printf(" 0. Logout\n");
        printf("---------------------------\n");
        printf("Enter choice: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            continue;
        }
        switch (choice) {
            case 1: viewActiveJobs(); break;
            case 2: viewInventoryTech(); break;
            case 3: inspectAndRequestExtraParts(); break;
            case 4: technicianRequestPartFromManager(); break;
            case 5: updateRepairStatus(); break;
            case 6: resetPassword(2); break;
            case 0: return;
            default: break;
        }
    }
}

void adminMenu() {
    int choice; 
    while (1) { 
        printf("\n---- MANAGER / ADMIN PORTAL ----\n"); 
        printf(" 1. Log New Customer Vehicle Intake\n"); 
        printf(" 2. View All Active Mechanical Jobs\n"); 
        printf(" 3. Manager Inventory & Dealer Ordering System\n");
        printf(" 4. Update Active Repair Job Phase Status\n"); 
        printf(" 5. Delete Vehicle Record\n"); 
        printf(" 6. Save System Logs & Reports\n"); 
        printf(" 7. View Monthly Sales & Net Profit Analysis\n"); 
        printf(" 8. Reset System Access Password\n"); 
        printf(" 0. Logout\n");
        printf("------------------------------------------\n"); 
        printf("Enter choice: "); 
        if (scanf("%d", &choice) != 1) { 
            while (getchar() != '\n'); 
            continue; 
        } 
        switch (choice) { 
            case 1: logNewVehicle(); break; 
            case 2: viewActiveJobs(); break; 
            case 3: managerInventoryAndDealerOrders(); break;
            case 4: updateRepairStatus(); break; 
            case 5: deleteVehicleRecord(); break; 
            case 6: saveLogsToFile(); break; 
            case 7: viewFinancialReport(); break; 
            case 8: resetPassword(1); break; 
            case 0: return; 
            default: break; 
        } 
    } 
}

int main() { 
    checkFirstTime(); 
    
    // Load persisted data on startup
    loadCustomerAccounts();
    loadInventoryData();
    loadPartRequestsData();

    while (1) { 
        printf("\n===== GARAGE SYSTEM PORTAL =====\n");
        printf("1. Manager / Admin\n");
        printf("2. Technician\n");
        printf("3. Customer\n");
        printf("0. Shutdown System\n");
        printf("--------------------------------\n");
        printf("Select Role: ");

        int roleChoice;
        if (scanf("%d", &roleChoice) != 1) {
            while (getchar() != '\n') {}
            continue;
        }

        if (roleChoice == 0) {
            saveCustomerAccounts();
            saveInventoryData();
            savePartRequestsData();
            printf("Shutting down system. All data successfully persisted. Goodbye!\n");
            exit(0);
        } else if (roleChoice == 1) {
            if (LoginUser(1)) adminMenu();
        } else if (roleChoice == 2) {
            if (LoginUser(2)) technicianMenu();
        } else if (roleChoice == 3) {
            if (LoginUser(3)) customerMenu();
        } else {
            printf("Invalid selection.\n");
        }
    } 
    return 0; 
}