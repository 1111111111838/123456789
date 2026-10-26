# Unisoc Dump Tool

כלי Windows (C) לשאיבת **Dump אמיתי** מזיכרון ה‑Flash של מכשירים מבוססי
Unisoc/Spreadtrum ומערכת **Mocor RTOS** (בדגש על טלפונים פשוטים / Feature
Phones), עם ממשק גרפי בהשראת Material Design.

> **הבהרת אמינות (חשובה):** הכלי קורא נתונים אמיתיים מהמכשיר באמצעות פרוטוקול
> ה‑Download המתועד של Spreadtrum (BSL). **ברירת המחדל היא ללא FDL** — קריאה
> ישירה דרך ה‑Boot ROM (`BSL_CMD_READ_FLASH`). האם שבב מסוים עונה לקריאה ישירה
> תלוי בחומרה; אם לא — ניתן לספק קובצי **FDL** (קנייניים, אינם מצורפים) כ‑fallback.
> הכלי **אינו ממציא** פקודות או FDL. היכן שחסר מידע להשלמת הקריאה לשבב — הדבר
> מוצג במפורש (ראו `docs/MISSING.md`).

הכלי **קורא בלבד**: אין כתיבה, מחיקה, פרמוט, שינוי מחיצות או צריבת קושחה.

---

## מצב מימוש (מה אמיתי, מה מאומת)

| רכיב | סטטוס |
|------|-------|
| פרוטוקול BSL (מסגור HDLC, CRC16 של Boot ROM, Checksum של FDL) | מומש ונבדק ביחידות |
| Handshake מול Boot ROM, CONNECT, VER | מומש |
| טעינת FDL1/FDL2 (START/MIDST/END/EXEC) | מומש |
| קריאת Flash לינארית (`READ_FLASH`) | מומש |
| קריאת Flash מחולקת (`READ_FLASH_START/MIDST/END`) | מומש |
| זיהוי אוטומטי של פורט Download (SetupAPI, VID 0x1782) | מומש (Windows) |
| כתיבת Binary זורמת + SHA‑256 + Manifest + יומן טווחים | מומש ונבדק |
| מצב סימולציה מלא (מופרד לחלוטין מהאמת) | מומש ונבדק |
| ממשק Material ב‑Win32 (Custom Drawing) | מומש |
| מתקין + חבילת דרייברים | ראו `docs/DRIVERS.md` ו‑`installer/` |
| אימות על חומרה פיזית | **לא בוצע** — ראו `docs/TESTED_DEVICES.md` |

מנוע הקריאה נבדק מקצה‑לקצה מול **מכשיר מסומלץ** הממש את אותו פרוטוקול, כולל
השוואה בייט‑בבייט של ה‑Dump שנקרא ו‑SHA‑256. בדיקות היחידה מאמתות את ה‑SHA‑256
(מול וקטורי FIPS), את ה‑CRC, את מסגור ה‑HDLC, את לולאת הקריאה ואת כתיבת הקבצים.

לפי דרישת המפרט: Dump נחשב "מלא" רק אם כל הטווח המבוקש נקרא בפועל; אחרת הוא מסומן
`INCOMPLETE`. נתוני סימולציה מסומנים תמיד `SIMULATED` ולעולם אינם מוצגים כאמיתיים.

---

## בנייה

### Visual Studio (מומלץ ל‑Windows)
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
פותחים את `build/UnisocDumpTool.sln` לעבודה ב‑IDE. התוצר:
`build/Release/UnisocDumpTool.exe`.

### שורת פקודה (MinGW‑w64)
```
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

### בדיקות (כל פלטפורמה)
```
cmake -S . -B build
cmake --build build --target udt_tests
ctest --test-dir build --output-on-failure
```

הכלי מאמת שהפרויקט אכן נבנה כ‑exe של Windows גם בבנייה מוצלבת עם MinGW
(`PE32+ GUI`), ושכל בדיקות הליבה עוברות.

---

## שימוש

1. התקינו את הדרייברים (ראו `docs/DRIVERS.md` / המתקין ב‑`installer/`).
2. חברו את המכשיר במצב **Download/Boot ROM** (לרוב בלחיצת מקש מסוים בעת חיבור
   ה‑USB, תלוי מכשיר).
3. הפעילו את `UnisocDumpTool.exe`. הכלי מזהה את הפורט אוטומטית ומציג כרטיס מכשיר.
4. בחרו תיקיית יעד (ברירת מחדל: `Documents\UnisocDumps`).
5. לשאיבה אמיתית: לחצו **התחל שאיבה** ואשרו. **ברירת המחדל היא ללא FDL** —
   הכלי קורא ישירות דרך ה‑Boot ROM (`BSL_CMD_READ_FLASH`) ומשתמש בגודל קריאה
   ברירת‑מחדל (8MB) אם לא הוגדר אחרת. ניתן (לא חובה) לערוך `UnisocDumpTool.ini`
   כדי לקבוע גודל/כתובת מדויקים, או — רק אם שבב מסוים אינו מאפשר קריאה ישירה —
   להצביע על קובצי FDL כ‑fallback.
6. לבדיקה ללא חומרה: לחצו **שאיבת סימולציה** — זורם דרך כל הצנרת ומייצר Dump
   מסומלץ מסומן ככזה.

### מבנה תיקיית הפלט
```
UnisocDumps/
└── <chip>_2026-10-26_203000/
    ├── device_info.txt
    ├── flash_dump.bin
    ├── dump_manifest.txt      (מצב, טווחים, זמנים, SHA-256, אמיתי/סימולציה)
    ├── dump_sha256.txt        (תואם sha256sum)
    └── INCOMPLETE.txt         (רק אם הקריאה נקטעה)
```

---

## ארכיטקטורה (מודולים נפרדים)

```
src/
├── common/     logging, sha256, crc16, appconfig
├── comm/       transport (ממשק מופשט), serial (COM אמיתי ב-Windows)
├── protocol/   hdlc (מסגור), sprd_proto (פקודות BSL), fdl (טעינת לודר)
├── detect/     chipdb (טבלת שבבים, נקודת הרחבה), device_detect (SetupAPI)
├── flash/      flash_dump (מנוע הקריאה: Handshake→FDL→קריאה→כתיבה)
├── fileout/    dump_writer (Binary זורם, SHA-256, Manifest, טווחים)
├── sim/        sim_device (מכשיר מסומלץ — מצב בדיקה בלבד)
└── ui/         ui_theme, ui_main (Win32 Material), resource/app.rc/manifest
```

הוספת דור חדש של Unisoc = הוספת שורת פרופיל ב‑`src/detect/chipdb.c` (ובמידת הצורך
טיפול בפקודות ייחודיות ב‑`sprd_proto`), בלי לגעת בשאר המודולים.

---

## מסמכים נוספים
- `docs/PROTOCOL.md` — תיאור הפרוטוקול והמקורות ההנדסיים שעליהם הוא מבוסס.
- `docs/MISSING.md` — מה חסר כדי להשלים קריאה לכל שבב (בעיקר FDL).
- `docs/DRIVERS.md` — אריזת דרייברים, חתימות, מגבלות חוקיות.
- `docs/TESTED_DEVICES.md` — מכשירים שנבדקו בפועל (כרגע: אף אחד; תבנית לתיעוד).
- `docs/THIRD_PARTY.md` — רישיונות ותלויות.
- `docs/UI.md` — עקרונות העיצוב והפלטה.

## רישיון
ראו `LICENSE`. רכיבי צד‑שלישי ורישיונותיהם ב‑`docs/THIRD_PARTY.md`.
