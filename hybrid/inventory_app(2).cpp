/*
 * ============================================================
 *   Hybrid Inventory Manager — single file build
 *   Compile:  g++ -std=c++17 -o inventory_app inventory_app.cpp
 *   Run:      ./inventory_app
 * ============================================================
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
#include <cstring>
#include <cstdio>

/* ============================================================
 *  SECTION 1 — C DATA STRUCTURE & CONSTANTS
 * ============================================================ */

#define MAX_NAME_LEN 40
#define DB_FILE      "inventory.dat"

typedef struct {
    int   id;
    char  name[MAX_NAME_LEN];
    int   quantity;
    float price;
    int   is_deleted;   /* 0 = active, 1 = soft-deleted */
} Item;

/* ============================================================
 *  SECTION 2 — C BACKEND  (file I/O with fread/fwrite/fseek)
 * ============================================================ */

/* Returns byte offset of the record with given id, -1 if not found.
   Copies the record into *out when found (if out != NULL).         */
static long find_offset(FILE *fp, int id, Item *out)
{
    Item tmp;
    std::rewind(fp);
    while (std::fread(&tmp, sizeof(Item), 1, fp) == 1) {
        if (tmp.id == id) {
            long pos = std::ftell(fp) - (long)sizeof(Item);
            if (out) *out = tmp;
            return pos;
        }
    }
    return -1L;
}

/* Add a new item — rejects duplicates. Returns 1 on success. */
static int add_item(const Item *item)
{
    if (!item || item->id <= 0) return 0;

    FILE *rfp = std::fopen(DB_FILE, "rb");
    if (rfp) {
        long pos = find_offset(rfp, item->id, NULL);
        std::fclose(rfp);
        if (pos >= 0) return 0;          /* duplicate ID */
    }

    FILE *fp = std::fopen(DB_FILE, "ab");
    if (!fp) return 0;
    int ok = (std::fwrite(item, sizeof(Item), 1, fp) == 1);
    std::fclose(fp);
    return ok;
}

/* Read one active item by id into *out. Returns 1 on success. */
static int get_item(int id, Item *out)
{
    if (!out || id <= 0) return 0;

    FILE *fp = std::fopen(DB_FILE, "rb");
    if (!fp) return 0;

    Item tmp;
    long pos = find_offset(fp, id, &tmp);
    std::fclose(fp);

    if (pos < 0 || tmp.is_deleted) return 0;
    *out = tmp;
    return 1;
}

/* Overwrite an existing record in-place. Returns 1 on success. */
static int update_item(int id, const Item *updated)
{
    if (!updated || id <= 0) return 0;

    FILE *fp = std::fopen(DB_FILE, "r+b");
    if (!fp) return 0;

    long pos = find_offset(fp, id, NULL);
    if (pos < 0) { std::fclose(fp); return 0; }

    std::fseek(fp, pos, SEEK_SET);
    int ok = (std::fwrite(updated, sizeof(Item), 1, fp) == 1);
    std::fclose(fp);
    return ok;
}

/* Soft-delete: sets is_deleted = 1 in the file. Returns 1 on success. */
static int delete_item(int id)
{
    if (id <= 0) return 0;

    FILE *fp = std::fopen(DB_FILE, "r+b");
    if (!fp) return 0;

    Item tmp;
    long pos = find_offset(fp, id, &tmp);
    if (pos < 0) { std::fclose(fp); return 0; }

    tmp.is_deleted = 1;
    std::fseek(fp, pos, SEEK_SET);
    int ok = (std::fwrite(&tmp, sizeof(Item), 1, fp) == 1);
    std::fclose(fp);
    return ok;
}

/* Copy up to max_items active records into buffer.
   Returns the number of items copied.              */
static int list_items(Item *buffer, int max_items)
{
    if (!buffer || max_items <= 0) return 0;

    FILE *fp = std::fopen(DB_FILE, "rb");
    if (!fp) return 0;

    int  count = 0;
    Item tmp;
    while (count < max_items && std::fread(&tmp, sizeof(Item), 1, fp) == 1)
        if (!tmp.is_deleted)
            buffer[count++] = tmp;

    std::fclose(fp);
    return count;
}

/* ============================================================
 *  SECTION 3 — C++ FRONTEND  (InventoryManager class + STL)
 * ============================================================ */

class InventoryManager {
public:
    void run();

private:
    /* menu actions */
    void menu_add();
    void menu_view();
    void menu_update();
    void menu_delete();
    void menu_list();

    /* validated input helpers */
    static int         read_pos_int   (const std::string &prompt);
    static int         read_nonneg_int(const std::string &prompt);
    static float       read_nonneg_flt(const std::string &prompt);
    static std::string read_name      (const std::string &prompt);

    /* display helpers */
    static void divider();
    static void print_header();
    static void print_row(const Item &it);
};

/* ── flush bad cin state ──────────────────────────────────── */
static void flush_cin()
{
    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

/* ── input helpers ───────────────────────────────────────── */
int InventoryManager::read_pos_int(const std::string &p)
{
    int v;
    while (true) {
        std::cout << p;
        if (std::cin >> v && v > 0) { flush_cin(); return v; }
        std::cout << "  [!] Must be a positive integer. Try again.\n";
        flush_cin();
    }
}

int InventoryManager::read_nonneg_int(const std::string &p)
{
    int v;
    while (true) {
        std::cout << p;
        if (std::cin >> v && v >= 0) { flush_cin(); return v; }
        std::cout << "  [!] Must be 0 or greater. Try again.\n";
        flush_cin();
    }
}

float InventoryManager::read_nonneg_flt(const std::string &p)
{
    float v;
    while (true) {
        std::cout << p;
        if (std::cin >> v && v >= 0.0f) { flush_cin(); return v; }
        std::cout << "  [!] Must be 0.00 or greater. Try again.\n";
        flush_cin();
    }
}

std::string InventoryManager::read_name(const std::string &p)
{
    std::string v;
    while (true) {
        std::cout << p;
        std::getline(std::cin, v);
        if (!v.empty() && (int)v.size() < MAX_NAME_LEN) return v;
        if (v.empty())
            std::cout << "  [!] Name cannot be empty.\n";
        else
            std::cout << "  [!] Name too long (max " << MAX_NAME_LEN - 1 << " chars).\n";
    }
}

/* ── display helpers ─────────────────────────────────────── */
void InventoryManager::divider()
{
    std::cout << std::string(58, '-') << '\n';
}

void InventoryManager::print_header()
{
    divider();
    std::cout << std::left
              << std::setw(6)  << "ID"
              << std::setw(22) << "Name"
              << std::setw(10) << "Qty"
              << std::setw(12) << "Price" << '\n';
    divider();
}

void InventoryManager::print_row(const Item &it)
{
    std::cout << std::left
              << std::setw(6)  << it.id
              << std::setw(22) << it.name
              << std::setw(10) << it.quantity
              << std::fixed << std::setprecision(2)
              << std::setw(12) << it.price << '\n';
}

/* ── menu actions ────────────────────────────────────────── */
void InventoryManager::menu_add()
{
    std::cout << "\n--- Add Item ---\n";
    Item it{};
    it.id = read_pos_int("  ID       : ");
    std::string nm = read_name("  Name     : ");
    std::strncpy(it.name, nm.c_str(), MAX_NAME_LEN - 1);
    it.quantity   = read_nonneg_int("  Quantity : ");
    it.price      = read_nonneg_flt("  Price    : ");
    it.is_deleted = 0;

    add_item(&it)
        ? std::cout << "  [OK] Item " << it.id << " added.\n"
        : std::cout << "  [!] Failed — ID may already exist.\n";
}

void InventoryManager::menu_view()
{
    std::cout << "\n--- View Item ---\n";
    int id = read_pos_int("  ID: ");
    Item it{};
    if (!get_item(id, &it)) {
        std::cout << "  [!] Not found or deleted.\n";
        return;
    }
    print_header();
    print_row(it);
    divider();
}

void InventoryManager::menu_update()
{
    std::cout << "\n--- Update Item ---\n";
    int id = read_pos_int("  ID to update: ");

    Item ex{};
    if (!get_item(id, &ex)) {
        std::cout << "  [!] Not found or deleted.\n";
        return;
    }

    Item up = ex;
    std::cout << "  (Press Enter to keep current value)\n";

    /* name */
    std::cout << "  New name [" << ex.name << "]: ";
    std::string nm; std::getline(std::cin, nm);
    if (!nm.empty() && (int)nm.size() < MAX_NAME_LEN)
        std::strncpy(up.name, nm.c_str(), MAX_NAME_LEN - 1);

    /* quantity */
    std::cout << "  New quantity [" << ex.quantity << "]: ";
    std::string qs; std::getline(std::cin, qs);
    if (!qs.empty()) {
        try {
            int q = std::stoi(qs);
            up.quantity = (q >= 0) ? q : ex.quantity;
        } catch (...) {}
    }

    /* price */
    std::cout << "  New price [" << std::fixed << std::setprecision(2)
              << ex.price << "]: ";
    std::string ps; std::getline(std::cin, ps);
    if (!ps.empty()) {
        try {
            float p = std::stof(ps);
            up.price = (p >= 0.0f) ? p : ex.price;
        } catch (...) {}
    }

    update_item(id, &up)
        ? std::cout << "  [OK] Item " << id << " updated.\n"
        : std::cout << "  [!] Update failed.\n";
}

void InventoryManager::menu_delete()
{
    std::cout << "\n--- Delete Item ---\n";
    int id = read_pos_int("  ID to delete: ");
    delete_item(id)
        ? std::cout << "  [OK] Item " << id << " deleted.\n"
        : std::cout << "  [!] Not found.\n";
}

void InventoryManager::menu_list()
{
    std::cout << "\n--- All Active Items ---\n";

    const int MAX_BUF = 1024;
    Item raw[MAX_BUF];
    int  cnt = list_items(raw, MAX_BUF);

    if (cnt == 0) { std::cout << "  (no items)\n"; return; }

    /* STL #1 — vector */
    std::vector<Item> items(raw, raw + cnt);

    /* STL #2 — sort by id */
    std::sort(items.begin(), items.end(),
              [](const Item &a, const Item &b){ return a.id < b.id; });

    print_header();
    for (const auto &it : items) print_row(it);
    divider();
    std::cout << "  Total: " << cnt << " item(s)\n";
}

/* ── main event loop ─────────────────────────────────────── */
void InventoryManager::run()
{
    std::cout << "\n==========================================\n";
    std::cout << "       Hybrid Inventory Manager           \n";
    std::cout << "==========================================\n";

    while (true) {
        std::cout << "\n  1. Add Item\n"
                     "  2. View Item\n"
                     "  3. Update Item\n"
                     "  4. Delete Item\n"
                     "  5. List All Items\n"
                     "  6. Exit\n"
                     "  Choice: ";

        int ch;
        if (!(std::cin >> ch)) { flush_cin(); continue; }
        flush_cin();

        switch (ch) {
            case 1: menu_add();    break;
            case 2: menu_view();   break;
            case 3: menu_update(); break;
            case 4: menu_delete(); break;
            case 5: menu_list();   break;
            case 6: std::cout << "Goodbye!\n"; return;
            default: std::cout << "  [!] Invalid choice.\n";
        }
    }
}

/* ============================================================
 *  SECTION 4 — ENTRY POINT
 * ============================================================ */

int main()
{
    InventoryManager mgr;
    mgr.run();
    return 0;
}
