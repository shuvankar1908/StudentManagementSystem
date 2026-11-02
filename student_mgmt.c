/*
 * PBL: Structure-oriented Persistent Data Management
 * Language : C
 *
 * Features
 *  - Login / registration (user id + password, password stored as a hash)
 *  - Menu driven interface
 *  - Student record (ID, NAME, FEES) kept in a struct
 *  - Course allotment (DS, AI, ML, Web) only after the fees are paid
 *  - Result declaration (Excellent / Good / Fail) after course delivery
 *  - Persistent storage using binary files (students.dat, users.dat)
 *  - Reports: course-wise list and result summary
 *
 * Default admin account (created on first run):  admin / admin123
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#define STUDENT_FILE "students.dat"
#define USER_FILE    "users.dat"
#define NAME_LEN     50
#define USER_LEN     20
#define PASS_LEN     30
#define NUM_COURSES  4
#define COURSE_FEE   15000.0f   /* minimum fees to be paid before allotment */

/* ---------- data structures ---------- */

typedef enum { GRADE_NONE = 0, GRADE_EXCELLENT, GRADE_GOOD, GRADE_FAIL } Grade;

typedef struct {
    int   id;
    char  name[NAME_LEN];
    float fees;      /* total fees paid so far            */
    int   course;    /* 0 = not allotted, 1..4 = course   */
    int   grade;     /* Grade enum                        */
} Student;

typedef struct {
    char         username[USER_LEN];
    unsigned int hash;
} User;

static const char *COURSES[NUM_COURSES + 1] = { "-", "DS", "AI", "ML", "Web" };
static const char *GRADES[4] = { "Pending", "Excellent", "Good", "Fail" };

static Student *students = NULL;   /* dynamic array of students */
static int      count    = 0;
static int      cap      = 0;

/* ---------- input helpers ---------- */

static void save_students(void);

static void read_line(const char *prompt, char *buf, int size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(buf, size, stdin) == NULL) {
        save_students();
        printf("\nInput closed. Data saved. Exiting.\n");
        exit(0);
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt, int *out)
{
    char buf[64], *end;
    long v;
    read_line(prompt, buf, sizeof buf);
    v = strtol(buf, &end, 10);
    if (end == buf || *end != '\0') {
        printf("  Invalid number.\n");
        return 0;
    }
    *out = (int)v;
    return 1;
}

static int read_float(const char *prompt, float *out)
{
    char buf[64], *end;
    float v;
    read_line(prompt, buf, sizeof buf);
    v = strtof(buf, &end);
    if (end == buf || *end != '\0') {
        printf("  Invalid amount.\n");
        return 0;
    }
    *out = v;
    return 1;
}

static void read_password(const char *prompt, char *buf, int size)
{
    printf("%s", prompt);
    fflush(stdout);
#ifdef _WIN32
    {
        int i = 0, c;
        while ((c = _getch()) != '\r' && c != '\n' && i < size - 1) {
            if (c == '\b') {
                if (i > 0) { i--; printf("\b \b"); }
            } else {
                buf[i++] = (char)c;
                putchar('*');
            }
        }
        buf[i] = '\0';
        printf("\n");
    }
#else
    {
        struct termios oldt, newt;
        int hidden = (tcgetattr(STDIN_FILENO, &oldt) == 0);
        if (hidden) {
            newt = oldt;
            newt.c_lflag &= ~(tcflag_t)ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        }
        if (fgets(buf, size, stdin) == NULL) buf[0] = '\0';
        buf[strcspn(buf, "\r\n")] = '\0';
        if (hidden) {
            tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
            printf("\n");
        }
    }
#endif
}

/* ---------- user authentication ---------- */

static unsigned int hash_password(const char *user, const char *pass)
{
    unsigned int h = 5381;
    const char *p;
    for (p = user; *p; p++) h = h * 33u + (unsigned char)*p;   /* salt */
    for (p = pass; *p; p++) h = h * 33u + (unsigned char)*p;
    return h;
}

static void init_users(void)
{
    FILE *fp = fopen(USER_FILE, "rb");
    if (fp) { fclose(fp); return; }
    fp = fopen(USER_FILE, "wb");
    if (!fp) { printf("Cannot create %s\n", USER_FILE); exit(1); }
    User u;
    memset(&u, 0, sizeof u);
    strcpy(u.username, "admin");
    u.hash = hash_password("admin", "admin123");
    fwrite(&u, sizeof u, 1, fp);
    fclose(fp);
}

static int user_exists(const char *name)
{
    User u;
    FILE *fp = fopen(USER_FILE, "rb");
    if (!fp) return 0;
    while (fread(&u, sizeof u, 1, fp) == 1)
        if (strcmp(u.username, name) == 0) { fclose(fp); return 1; }
    fclose(fp);
    return 0;
}

static int check_login(const char *name, const char *pass)
{
    User u;
    FILE *fp = fopen(USER_FILE, "rb");
    if (!fp) return 0;
    while (fread(&u, sizeof u, 1, fp) == 1)
        if (strcmp(u.username, name) == 0) {
            fclose(fp);
            return u.hash == hash_password(name, pass);
        }
    fclose(fp);
    return 0;
}

static void register_user(void)
{
    char name[USER_LEN + 2], p1[PASS_LEN + 2], p2[PASS_LEN + 2];
    User u;
    FILE *fp;

    read_line("  New user id : ", name, sizeof name);
    if (strlen(name) < 3 || strlen(name) > USER_LEN - 1 || strchr(name, ' ')) {
        printf("  User id must be 3-%d characters, no spaces.\n", USER_LEN - 1);
        return;
    }
    if (user_exists(name)) { printf("  User id already exists.\n"); return; }
    read_password("  Password    : ", p1, sizeof p1);
    if (strlen(p1) < 4) { printf("  Password must be at least 4 characters.\n"); return; }
    read_password("  Confirm     : ", p2, sizeof p2);
    if (strcmp(p1, p2) != 0) { printf("  Passwords do not match.\n"); return; }

    memset(&u, 0, sizeof u);
    strcpy(u.username, name);
    u.hash = hash_password(name, p1);
    fp = fopen(USER_FILE, "ab");
    if (!fp) { printf("  Cannot write user file.\n"); return; }
    fwrite(&u, sizeof u, 1, fp);
    fclose(fp);
    printf("  User '%s' registered successfully.\n", name);
}

static int do_login(char *logged_user)
{
    int attempt;
    for (attempt = 1; attempt <= 3; attempt++) {
        char name[USER_LEN + 2], pass[PASS_LEN + 2];
        read_line("  User id  : ", name, sizeof name);
        read_password("  Password : ", pass, sizeof pass);
        if (check_login(name, pass)) {
            strcpy(logged_user, name);
            printf("  Login successful. Welcome, %s!\n", name);
            return 1;
        }
        printf("  Invalid user id or password (attempt %d of 3).\n", attempt);
    }
    printf("  Too many failed attempts.\n");
    return 0;
}

static int auth_menu(char *logged_user)
{
    for (;;) {
        int ch;
        printf("\n===== STUDENT MANAGEMENT SYSTEM =====\n");
        printf(" 1. Login\n 2. Register new user\n 0. Exit\n");
        if (!read_int("Enter choice: ", &ch)) continue;
        if (ch == 1) { if (do_login(logged_user)) return 1; }
        else if (ch == 2) register_user();
        else if (ch == 0) return 0;
        else printf("  Invalid choice.\n");
    }
}

/* ---------- persistent storage for students ---------- */

static void load_students(void)
{
    Student s;
    FILE *fp = fopen(STUDENT_FILE, "rb");
    if (!fp) return;                       /* first run: no file yet */
    while (fread(&s, sizeof s, 1, fp) == 1) {
        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            students = realloc(students, (size_t)cap * sizeof(Student));
            if (!students) { printf("Out of memory\n"); exit(1); }
        }
        students[count++] = s;
    }
    fclose(fp);
}

static void save_students(void)
{
    FILE *fp = fopen(STUDENT_FILE, "wb");
    if (!fp) { printf("  ERROR: cannot write %s\n", STUDENT_FILE); return; }
    if (count > 0) fwrite(students, sizeof(Student), (size_t)count, fp);
    fclose(fp);
}

/* ---------- student helpers ---------- */

static int find_index(int id)
{
    int i;
    for (i = 0; i < count; i++)
        if (students[i].id == id) return i;
    return -1;
}

static int cmp_id(const void *a, const void *b)
{
    return ((const Student *)a)->id - ((const Student *)b)->id;
}

static void print_header(void)
{
    printf("\n%-6s %-24s %-10s %-9s %-8s %-10s\n",
           "ID", "NAME", "FEES", "FEE STATUS", "COURSE", "RESULT");
    printf("---------------------------------------------------------------------\n");
}

static void print_student(const Student *s)
{
    printf("%-6d %-24s %-10.2f %-10s %-8s %-10s\n",
           s->id, s->name, s->fees,
           s->fees >= COURSE_FEE ? "Paid" : "Pending",
           COURSES[s->course], GRADES[s->grade]);
}

static int ask_id(int *id)
{
    if (!read_int("  Enter student ID: ", id)) return 0;
    return 1;
}

/* ---------- menu operations ---------- */

static void add_student(void)
{
    Student s;
    int id;
    float fees;

    memset(&s, 0, sizeof s);
    if (!read_int("  Student ID (positive number): ", &id)) return;
    if (id <= 0) { printf("  ID must be positive.\n"); return; }
    if (find_index(id) >= 0) { printf("  ID %d already exists.\n", id); return; }

    read_line("  Student name: ", s.name, sizeof s.name);
    if (s.name[0] == '\0') { printf("  Name cannot be empty.\n"); return; }

    if (!read_float("  Fees paid now (0 if none): ", &fees)) return;
    if (fees < 0) { printf("  Fees cannot be negative.\n"); return; }

    s.id = id;
    s.fees = fees;

    if (count == cap) {
        cap = cap ? cap * 2 : 16;
        students = realloc(students, (size_t)cap * sizeof(Student));
        if (!students) { printf("Out of memory\n"); exit(1); }
    }
    students[count++] = s;
    qsort(students, (size_t)count, sizeof(Student), cmp_id);
    save_students();
    printf("  Student registered. Fee status: %s (course fee = %.2f)\n",
           fees >= COURSE_FEE ? "Paid" : "Pending", COURSE_FEE);
}

static void view_all(void)
{
    int i;
    if (count == 0) { printf("  No students registered.\n"); return; }
    print_header();
    for (i = 0; i < count; i++) print_student(&students[i]);
    printf("Total students: %d\n", count);
}

static void search_student(void)
{
    int id, idx;
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    print_header();
    print_student(&students[idx]);
}

static void update_name(void)
{
    int id, idx;
    char name[NAME_LEN];
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    read_line("  New name: ", name, sizeof name);
    if (name[0] == '\0') { printf("  Name cannot be empty.\n"); return; }
    strcpy(students[idx].name, name);
    save_students();
    printf("  Name updated.\n");
}

static void pay_fees(void)
{
    int id, idx;
    float amt;
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    if (!read_float("  Amount paid: ", &amt)) return;
    if (amt <= 0) { printf("  Amount must be greater than zero.\n"); return; }
    students[idx].fees += amt;
    save_students();
    printf("  Payment recorded. Total paid: %.2f (%s)\n", students[idx].fees,
           students[idx].fees >= COURSE_FEE ? "Fees cleared" : "Still pending");
}

static void allot_course(void)
{
    int id, idx, c, i;
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    if (students[idx].fees < COURSE_FEE) {
        printf("  Fees pending (paid %.2f of %.2f). Course cannot be allotted.\n",
               students[idx].fees, COURSE_FEE);
        return;
    }
    if (students[idx].course != 0) {
        printf("  Student already allotted course %s.\n", COURSES[students[idx].course]);
        return;
    }
    printf("  Available courses:\n");
    for (i = 1; i <= NUM_COURSES; i++) printf("    %d. %s\n", i, COURSES[i]);
    if (!read_int("  Select course: ", &c)) return;
    if (c < 1 || c > NUM_COURSES) { printf("  Invalid course.\n"); return; }
    students[idx].course = c;
    save_students();
    printf("  %s allotted to %s.\n", COURSES[c], students[idx].name);
}

static void declare_result(void)
{
    int id, idx, g;
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    if (students[idx].course == 0) {
        printf("  No course allotted yet. Result cannot be declared.\n");
        return;
    }
    printf("  1. Excellent\n  2. Good\n  3. Fail\n");
    if (!read_int("  Select result: ", &g)) return;
    if (g < 1 || g > 3) { printf("  Invalid result.\n"); return; }
    students[idx].grade = g;
    save_students();
    printf("  Result declared: %s -> %s\n", students[idx].name, GRADES[g]);
}

static void delete_student(void)
{
    int id, idx, i;
    char ans[8];
    if (!ask_id(&id)) return;
    idx = find_index(id);
    if (idx < 0) { printf("  Student not found.\n"); return; }
    read_line("  Are you sure you want to delete (y/n)? ", ans, sizeof ans);
    if (tolower((unsigned char)ans[0]) != 'y') { printf("  Cancelled.\n"); return; }
    for (i = idx; i < count - 1; i++) students[i] = students[i + 1];
    count--;
    save_students();
    printf("  Student deleted.\n");
}

static void course_report(void)
{
    int c, i, n;
    for (c = 1; c <= NUM_COURSES; c++) {
        n = 0;
        printf("\nCourse %s:\n", COURSES[c]);
        for (i = 0; i < count; i++)
            if (students[i].course == c) {
                printf("   %-6d %-24s %s\n", students[i].id, students[i].name,
                       GRADES[students[i].grade]);
                n++;
            }
        if (n == 0) printf("   (no students)\n");
        else printf("   Total: %d\n", n);
    }
}

static void result_summary(void)
{
    int tally[4] = {0, 0, 0, 0}, noCourse = 0, i;
    for (i = 0; i < count; i++) {
        if (students[i].course == 0) noCourse++;
        else tally[students[i].grade]++;
    }
    printf("\n--- Result Summary ---\n");
    printf("  Excellent            : %d\n", tally[GRADE_EXCELLENT]);
    printf("  Good                 : %d\n", tally[GRADE_GOOD]);
    printf("  Fail                 : %d\n", tally[GRADE_FAIL]);
    printf("  Result pending       : %d\n", tally[GRADE_NONE]);
    printf("  No course allotted   : %d\n", noCourse);
    printf("  Total students       : %d\n", count);
}

/* ---------- main menu ---------- */

static void main_menu(const char *user)
{
    for (;;) {
        int ch;
        printf("\n======== MAIN MENU (user: %s) ========\n", user);
        printf(" 1. Register new student\n");
        printf(" 2. View all students\n");
        printf(" 3. Search student by ID\n");
        printf(" 4. Update student name\n");
        printf(" 5. Pay fees\n");
        printf(" 6. Allot course (after fees paid)\n");
        printf(" 7. Declare result\n");
        printf(" 8. Delete student\n");
        printf(" 9. Course-wise report\n");
        printf("10. Result summary\n");
        printf(" 0. Logout & save\n");
        if (!read_int("Enter choice: ", &ch)) continue;
        switch (ch) {
            case 1:  add_student();    break;
            case 2:  view_all();       break;
            case 3:  search_student(); break;
            case 4:  update_name();    break;
            case 5:  pay_fees();       break;
            case 6:  allot_course();   break;
            case 7:  declare_result(); break;
            case 8:  delete_student(); break;
            case 9:  course_report();  break;
            case 10: result_summary(); break;
            case 0:  save_students(); printf("  Data saved. Logged out.\n"); return;
            default: printf("  Invalid choice.\n");
        }
    }
}

int main(void)
{
    char user[USER_LEN + 2];

    init_users();
    load_students();

    if (auth_menu(user))
        main_menu(user);

    save_students();
    free(students);
    printf("Goodbye!\n");
    return 0;
}
