> # Mycelia CSS v1.2 “The Mesh”
> 
> Dieses Whitepaper beschreibt die aktuelle Architektur von **Mycelia CSS**, einem nativen Style- und Physics-Engine-Set, das Web-design, native Physik und grafische Assets in einem konsistenten Binär-Katalog vereint. Version 1.2 („The Mesh“) liefert:
> 
> 1. Einen Zero-Copy-nativen Kern mit `style_`-Tokens, `NativeStyle` und rekursiver `translate_impl`.
> 2. Eine Erweiterung der **Tailwind-artigen Katalogstruktur** um physikalische Direktiven, Bild-Templates und HTML-Multi-Seiten.
> 3. Eine Web-Generator-Pipeline (`write_assets.py` + `generate_html.py`), die mehrere HTML-Seiten, Navigation und JS-Animation aus der DLL produziert.
> 4. QA-, Live-Reload- und DLL-Bridges, die C#, C++ und Browser synchronisieren.
> 
> ---
> 
> ## 1. Kernidee: Design, Physik, Web in einer Binärquelle
> 
> Mycelia CSS ersetzt klassische Frontend-Pipelines durch eine einzige C++-DLL:
> 
> * Der native `i18n_engine` parst `tailwind_style_catalog.i18n`. Jeder `style_*`-Token kann CSS-Eigenschaften enthalten und physikalische Direktiven (`--mass`, `--friction`, `--spacing`, `--gravity`, …).
> * Drei Layer arbeiten zusammen: textuelle Tokens, binäre `CatalogSnapshot`s und eine `style_registry`, die `NativeStyle`-Structs befüllt und Physik direkt über `i18n_get_native_style` bzw. `TryGetNativeStyle` ausliefert.
> * `translate_impl` nutzt Platzhalter (`%0`, `%N`) sowie Style-Vererbung. Dadurch bleiben `@style_*`-Referenzen konsistent über Lokalisierung, logische Templates und GPU-Render zu `NativeStyle`.
> * Die DLL unterstützt `mmap` (bzw. Windows `CreateFileMapping`), `Atomic Swapping` von Snapshot-Pointern und einen Live-Wächter (`main.cpp`), der Änderungen am Textkatalog innerhalb von 500 ms erkennt und Hot-Reload auslöst, während andere Threads weiter mit dem alten Snapshot arbeiten.
> 
> ---
> 
> ## 2. Tailwind-ähnlicher Katalog mit Physik, Bildern und Templates
> 
> **tailwind_style_catalog.i18n** ist das „Single Source of Truth“. Neuerungen:
> 
> * `style_card-image`, `style_card-body-alt`, `style_nav-*`, `style_hero-banner` etc. liefern das komplette visuelle Vokabular. Die Style-Tokens enthalten nun `background-image`-URLs, `filter`, `box-shadow` und Alltags-Eigenschaften aus Single-Source-Design und Physik.
> * Die `style_cube-*`-Tokens setzen physikalische Werte, die `NativeStyle` zum Kochen bringt. So können C++-Compute-Shader, C#-Swarm-Schleifen oder das JavaScript im Browser identische Werte verwenden.
> * Neue Template-Token:
>   * `tpl_card` und `tpl_image-card` rendern einfache Textkarten oder Bildkarten mit `background-image:url('%0')`.
>   * `tpl_full-page` baut die fertige Desktop-Layout-Struktur (Navbar, Hero, Card Grid) inklusive `<a>`-Links auf `index.html`, `services.html`, `matrix.html`, `insights.html`.
>   * `tpl_main-layout` bleibt als Beispiel für Hero-Seiten bestehen und wird intern von den Generatoren verwendet.
> 
> Die Template-Engine ersetzt `@style_*` durch sichere CSS-Klassen, fügt `<style>`-Blöcke ein und liefert am Ende HTML-Strings, die direkt in statische Seiten geschrieben werden können.
> 
> ---
> 
> ## 3. Web-Generator-Pipeline
> 
> Die Pipeline besteht aus zwei Python-Skripten:
> 
> 1. **`write_assets.py`** erstellt placeholder SVGs unter `www/assets/`. Diese SVGs entsprechen den Bild-URLs der neuen `tpl_image-card`-Karten und brauchen keine externen Assets.
> 2. **`generate_html.py`** lädt die DLL, iteriert über `PAGE_DEFINITIONS`, rendert jeden Satz von Karten (`tpl_card` oder `tpl_image-card`) inklusive der Bild-URLs und schreibt vier HTML-Dateien in `www/`. Jede Datei:
>    * Enthält `<style>`-Blöcke, die alle `@style_*`-Referenzen auflösen.
>    * Beinhaltet `MyceliaPhysics` als JSON mit `mass`, `friction`, `spacing` für alle referenzierten `style_cube-*`-Token.
>    * Fügt ein JavaScript ein, das den hero-Cube (`#hero-ice`) animiert (Bounce/Scale ergibt sich aus `friction`/`spacing` und `mass`) und die Daten ins Console.log schreibt.
>    * Bietet eine funktionierende Navigation + CTA-Buttons, die zwischen `index.html`, `services.html`, `matrix.html`, `insights.html` wechseln.
> 
> Diese Pipeline ersetzt klassische Web-Builder: Du änderst `tailwind_style_catalog.i18n`, führst `python write_assets.py && python generate_html.py` aus, und der Webserver bekommt sofort aktualisierte Seiten mit nativen Styles und physikalisch korrekten Daten.
> 
> ---
> 
> ## 4. QA & Konsistenz
> 
> * `i18n_qa.py` validiert `style_card` (Border/Background) sowie Hover/Native Physics (`style_cube-heavy`, `style_cube-bounce`), einschließlich der `--mass`/`--friction`/`--spacing`-Werte.
> * `make` führt `i18n_qa.py` aus und scheitert, falls eine Assertion nicht erfüllt ist.
> * `main.cpp` bietet den Live-Reload-/Live-Inspect-Demo (`make run`) – der Watcher sieht `tailwind_style_catalog.i18n` an und remapped die Datei, während in der Demo das neue `style_cube-ice`-Profil ausgegeben wird.
> 
> ---
> 
> ## 5. Verwurzelte Cross-Platform-Use Cases
> 
> * **SubQG Matrix**: UI-Cubes werden nicht nur optisch gestylt, sondern beim Rendern durch `NativeStyle`-Physikparameter (Mass, Friction, Spacing) beschrieben. Diese Werte werden an C#-Swarm-Prozesse (Compute Shader) und an das JavaScript im Browser verteilt, sodass alle Ebenen denselben „Living Mesh“-Zustand sehen.
> * **Unity & .NET**: `I18n.cs` erweitert die DLL um `GetStyle` und `TryGetNativeStyle`. Die C#-Swarm-Updater nutzen die native Struktur (Zero Copy), während die Windows-Engine denselben Katalog remapt.
> * **Web & Electron**: Die von `generate_html.py` produzierten Seiten enthalten bereits die CSS-Definitionen plus ein animiertes Demo-Element. Änderungen im `.i18n` sprechen also sofort sowohl native als auch Web-View-Clients an.
> 
> ---
> 
> ## 6. Ausblick
> 
> 1. **Live-Reload auch für Web**: Setze einen FileSystemWatcher auf `www/index.html` & Co., um Änderungen an der DLL zu spiegeln und `MyceliaPhysics` in Echtzeit neu zu laden.
> 2. **Asset-Pipeline erweitern**: Tausche die SVG-Generatoren gegen echte Illustrationen aus, lade sie in `www/assets/` und verlinke sie über `tpl_image-card`.
> 3. **Binary-Verteilung**: Nutze `i18n_export_binary` (mit FNV1a32) für Releases und stelle die `.bin`-Datei gemeinsam mit den generierten Seiten bereit.
>
> ---
>
> ## Release Highlight
>
> * **The Mesh (v1.2)** bringt `write_assets.py` + `generate_html.py`, was vier statische Seiten mit Bildkarten, Navigation, `MyceliaPhysics` JSON und animiertem Hero erzeugt.
> * **Design + Physik aus einem Katalog**: `style_card-image`, `style_cube-*`, `tpl_image-card` steuern Bild, Layout und physikalische Parameter – die SVGs liegen unter `www/assets/`.
> * **QA & Hot Reload**: `make` läuft weiter über `i18n_qa.py` und `main.cpp` überwacht den Katalog alle 500 ms, sodass Änderungen sofort in Web und nativen Clients sichtbar sind.
>

