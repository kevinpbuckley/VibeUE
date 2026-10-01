# Project Settings Service Tests

Tests that INI and settings writes reach the files on disk. Run sequentially. After each write, read
the file on disk (not only through the service) and print the evidence behind each pass/fail. Put
every value back at the end.

---

## INI value on disk

Write a test key into a scratch section of the project's DefaultGame.ini with set_ini_value, then open Config/DefaultGame.ini on disk and show me the line. (Expected: success, the key=value line is in the file, and the file's other lines are unchanged.)

---

Read that key back with get_ini_value, then set it to a second value with set_ini_value and read it again. (Expected: the second read returns the second value, not the first: reads see the file as it is now, also after an earlier read.)

---

Remove the scratch section from the file by hand, confirm get_ini_value no longer returns the key, and confirm the file is as it was.

---

## Refused writes

Try set_ini_value with a section name that contains a line break, then with a config file path that ends in .txt instead of .ini. (Expected: both are refused with a reason, and no file is created or changed.)

---

## Arrays

Try set_ini_array on a scratch section of DefaultGame.ini that is not a settings class. (Expected: it refuses with a reason and writes nothing, rather than reporting a success that never reached the disk.)

---

Note the current value of ProjectPackagingSettings' DirectoriesToAlwaysStageAsNonUFS with get_settings_property, then use set_ini_array on "/Script/DeveloperToolSettings.ProjectPackagingSettings" to set it to one struct element, (Path="VibeUETest"). Show me the line in DefaultGame.ini on disk and what get_ini_array returns. Then put the old value back with set_settings_property. (Expected: success; the element is saved as a struct, not as a quoted string, and get_ini_array returns it starting with "(".)

---

## Settings objects

Change the project description (GeneralProjectSettings, Description) with set_settings_property, then show me the current value through get_settings_property and the Description line in DefaultGame.ini on disk. Then put the old description back.

---

Try set_settings_property with a property the class doesn't have. (Expected: refused, nothing changed.)

---
