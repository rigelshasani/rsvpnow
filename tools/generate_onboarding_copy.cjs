// Reuse the approved Study 19 translations; firmware has no browser dependency.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const root=path.resolve(__dirname,'..'),sketch=path.join(root,'sketches/firmware-study-19');
const read=f=>fs.readFileSync(path.join(sketch,f),'utf8');
const context=vm.createContext({});
vm.runInContext(read('translations.js').split('const languageNames')[0],context);
vm.runInContext(read('onboarding.js').split('Object.assign(uiCopy,lessonCopy')[0],context);
vm.runInContext(read('lessons.js').split('// Physical order')[0],context);
vm.runInContext(read('tuning.js').split('const tuningKeys')[0],context);
vm.runInContext(read('books.js').split('let bookUploadMethod')[0],context);
vm.runInContext(read('storage.js').split('// Browser-only scenarios')[0],context);
const copy=vm.runInContext('Object.assign({},uiCopy,lessonCopy,tuningCopy)',context);
copy.swipeHelpHold=['Hold text to read; release to pause.','Text halten zum Lesen; loslassen pausiert.','Ține textul pentru lectură; eliberează pentru pauză.'];
copy.speedAdvice=['Losing the meaning? Go slower. Feels easy? Try faster. Replay until comfortable.','Verlierst du den Sinn? Langsamer. Fällt es leicht? Schneller. Probiere es in Ruhe.','Pierzi sensul? Mai încet. E ușor? Mai repede. Repetă până te simți confortabil.'];
copy.sdFinishing=['Finishing…','Wird beendet…','Se încheie…'];
copy.sdBusyBody=['The card is still responding. Try the demo, or power off and reseat the card.','Die Karte antwortet noch. Demo testen oder ausschalten und Karte neu einsetzen.','Cardul încă răspunde. Încearcă demo sau oprește Nano și reintrodu cardul.'];
copy.sdSavePending=['Setup save pending','Einrichtung nicht gespeichert','Configurarea nu e salvată'];
copy.sdSaveRetry=['Retrying in the background.','Erneuter Versuch im Hintergrund.','Reîncercăm în fundal.'];
copy.guideSize=['Guide size','Zeichengröße','Mărime ghid'];
copy.textGap=['Text gap','Textabstand','Distanță text'];
copy.touchTest=['Touch test','Touch-Test','Test tactil'];
copy.touchSlide=['Slide across every cell','Über alle Felder streichen','Glisează peste fiecare celulă'];
copy.dictionary=['Dictionary','Wörterbuch','Dicționar'];
copy.dictionaryLesson=['Swipe up while paused, then tap Dictionary. Browse nearby words or search.','Im pausierten Buch nach oben wischen und Wörterbuch wählen. Wörter wechseln oder suchen.','Glisează în sus când lectura e oprită și apasă Dicționar. Schimbă cuvântul sau caută.'];
copy.dictionaryOffline=['Offline English','Englisch offline','Engleză offline'];
copy.dictionaryOfflineBody=['Install over Wi-Fi or copy the pack to SD. More languages are planned.','Per WLAN installieren oder Paket auf SD kopieren. Weitere Sprachen sind geplant.','Instalează prin Wi-Fi sau copiază pachetul pe SD. Plănuim să adăugăm și alte limbi.'];
copy.dictionaryOnline=['Look online','Online suchen','Caută online'];
copy.dictionaryOnlineBody=['Tap Look online for Wiktionary over Wi-Fi. Only the word is sent when you ask.','Online suchen nutzt Wiktionary per WLAN. Das Wort wird nur auf Anfrage gesendet.','Caută online folosește Wiktionary prin Wi-Fi. Cuvântul e trimis doar la cerere.'];
copy.swipeTitle=["Middle button.", "Mittlere Taste.", "Butonul din mijloc."];
copy.swipeBody=["Press the middle button at the top right for speed, brightness and navigation.", "Drücke oben rechts die mittlere Taste für Tempo, Helligkeit und Navigation.", "Apasă butonul din mijloc, sus în dreapta, pentru viteză, luminozitate și navigare."];
copy.swipeAccessible=["Press the middle button for controls", "Mittlere Taste für die Lesesteuerung", "Butonul din mijloc deschide comenzile"];
copy.swipeHelp=["Middle button: speed, brightness and navigation. Tap Menu for all settings.", "Mittlere Taste: Tempo, Helligkeit und Navigation. Menü öffnet alle Einstellungen.", "Butonul din mijloc: viteză, luminozitate și navigare. Meniu deschide toate setările."];
copy.powerHelp=["Open reading controls", "Lesesteuerung öffnen", "Deschide comenzile de lectură"];
copy.dictionaryLesson=["Press the middle button, then tap Dictionary. Browse nearby words or search.", "Drücke die mittlere Taste und wähle Wörterbuch. Wörter wechseln oder suchen.", "Apasă butonul din mijloc, apoi Dicționar. Schimbă cuvântul sau caută."];
copy.guidesUnavailableChunks=["Guides are unavailable in chunk mode. Switch to Word to use them.", "Hilfslinien sind im Gruppenmodus nicht verfügbar. Nutze dafür den Wortmodus.", "Ghidajele nu sunt disponibile în modul grup. Treci la Cuvânt pentru a le folosi."];
copy.displayHint=['Theme & sleep','Farben & Ruhe','Temă și repaus'];
copy.deviceHint=['Language, storage & updates','Sprache, Speicher & Updates','Limbă, stocare și actualizări'];
copy.displayExplain=['Choose your theme, sleep delay and screensaver. Brightness is in quick controls.','Wähle Farben, Ruhezeit und Bildschirmschoner. Helligkeit ist in der Lesesteuerung.','Alege tema, repausul și economizorul. Luminozitatea e în comenzile rapide.'];
copy.deviceExplain=['Change the interface language, check storage and update firmware.','Wähle die Menüsprache, prüfe den Speicher und aktualisiere die Firmware.','Schimbă limba interfeței, verifică stocarea și actualizează firmware-ul.'];
copy.menuLibrary=['Open a book or change its font and language with Book settings. Each book keeps its reading place.','Öffne ein Buch oder ändere Schrift und Sprache in den Bucheinstellungen. Die Lesestelle bleibt gespeichert.','Deschide o carte sau schimbă fontul și limba din setările cărții. Locul lecturii rămâne salvat.'];
// RSVP Nano branding; the Pocket Lines mark is retained only as the reading guide.
const keys=Object.keys(copy),name=k=>k[0].toUpperCase()+k.slice(1);
// Phrases looked up by their English wording; appended after the keyed rows.
const phrases=require('./ui-phrases.cjs');
for(const row of phrases)if(row.length!==3||row.some(t=>typeof t!=='string'||!t))throw new Error('Bad phrase row: '+JSON.stringify(row));
// The approved keyed translations win over a phrase with the same English.
const approved=new Set(keys.map(k=>copy[k][0]));
const seen=new Map();
for(const row of phrases){
    const earlier=seen.get(row[0]);
    if(earlier&&earlier.join('\u0000')!==row.join('\u0000'))throw new Error('Two translations for one phrase: '+JSON.stringify(row[0]));
    seen.set(row[0],row);
}
const rows=[...keys.map(k=>copy[k]),...[...seen.values()].filter(row=>!approved.has(row[0]))];
const english=new Map();
for(const [index,row] of rows.entries())if(!english.has(row[0]))english.set(row[0],index);
// Binary search in C++ compares bytes, so sort by UTF-8.
const byEnglish=[...english.entries()].sort((x,y)=>Buffer.compare(Buffer.from(x[0]),Buffer.from(y[0]))).map(e=>e[1]);
const target=path.join(root,'src/ui/screens');
fs.writeFileSync(path.join(target,'OnboardingCopy.h'),'// Generated by tools/generate_onboarding_copy.cjs\n#pragma once\n#include <initializer_list>\n#include <string>\n#include "ui/Ui.h"\nnamespace screens::modern {\nenum class Copy : uint16_t {\n'+keys.map(k=>'    '+name(k)).join(',\n')+'\n};\nstd::string_view copy(const ui::Context& ui, Copy key);\n// The interface language\'s wording for an English phrase; the phrase itself when there is none.\nstd::string_view tr(const ui::Context& ui, std::string_view english);\n// tr() with each "{}" replaced by the next value.\nstd::string trf(const ui::Context& ui, std::string_view english, std::initializer_list<std::string_view> values);\n}\n');
fs.writeFileSync(path.join(target,'OnboardingCopy.cpp'),'// Generated by tools/generate_onboarding_copy.cjs\n#include "OnboardingCopy.h"\n#include <algorithm>\n#include <iterator>\nnamespace screens::modern {\nnamespace {\nconstexpr const char* words[][3] = {\n'+rows.map(row=>'    {'+row.map(t=>JSON.stringify(t)).join(', ')+'}').join(',\n')+'\n};\n// Rows ordered by their English wording, for tr().\nconstexpr uint16_t byEnglish[] = {'+byEnglish.join(', ')+'};\nsize_t language(const ui::Context& ui) {\n    const auto locale=ui.locale();\n    return locale=="de" || locale.starts_with("de-") ? 1 : locale=="ro" || locale.starts_with("ro-") ? 2 : 0;\n}\n}\nstd::string_view copy(const ui::Context& ui, Copy key) {\n    return words[static_cast<size_t>(key)][language(ui)];\n}\nstd::string_view tr(const ui::Context& ui, std::string_view english) {\n    const size_t column=language(ui);\n    if (column==0 || english.empty()) return english;\n    const auto found=std::lower_bound(std::begin(byEnglish), std::end(byEnglish), english,\n        [](uint16_t row, std::string_view wanted) { return std::string_view{words[row][0]} < wanted; });\n    return found!=std::end(byEnglish) && std::string_view{words[*found][0]}==english ? std::string_view{words[*found][column]} : english;\n}\nstd::string trf(const ui::Context& ui, std::string_view english, std::initializer_list<std::string_view> values) {\n    const std::string_view pattern=tr(ui, english);\n    std::string out;\n    auto value=values.begin();\n    for (size_t i=0; i<pattern.size(); ++i) {\n        if (pattern[i]==\'{\' && i+1<pattern.size() && pattern[i+1]==\'}\' && value!=values.end()) { out+=*value++; ++i; }\n        else out+=pattern[i];\n    }\n    return out;\n}\n}\n');
console.log('Generated '+keys.length+' keyed entries and '+phrases.length+' phrases.');
