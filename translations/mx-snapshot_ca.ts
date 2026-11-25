<?xml version="1.0" ?><!DOCTYPE TS><TS version="2.1" language="ca">
<context>
    <name>Batchprocessing</name>
    <message>
        <location filename="../src/batchprocessing.cpp" line="95"/>
        <source>Error</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="96"/>
        <source>Current kernel doesn&apos;t support selected compression algorithm, please edit the configuration file and select a different algorithm.</source>
        <translation>El kernel actual no dona suport a l&apos;algoritme de compressió seleccionat, si us plau editeu el fitxer de configuració i trieu un algoritme diferent.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="116"/>
        <source>Warning: these services in /etc/systemd/system contain what look like passwords, tokens or keys, and will be included in the snapshot as they are:</source>
        <translation>Atenció: aquests serveis a /etc/systemd/system poden contenir contrasenyes, testimonis o claus, i seran inclosos a la instantània tal com estan.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="121"/>
        <source>Warning: these service files or folders could not be checked for passwords, tokens or keys, and will be included in the snapshot as they are:</source>
        <translation>Atenció: aquests fitxers de serveis o carpetes no s&apos;han pogut comprovar per si contenen contrasenyes, testimonis o claus i s&apos;inclouran a la instantània tal com estan.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="166"/>
        <source>The program will pause the build and open the boot menu in your text editor.</source>
        <translation>El programa aturarà la construcció i obrirà el menú d&apos;arrencada al vostre editor de text.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="179"/>
        <source>The boot-menu editor failed; the snapshot cannot continue with potentially unedited files.</source>
        <translation>L&apos;editor boot-menu ha fallat; la instantània no pot continuar amb fitxers potencialment no editats.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="207"/>
        <source>Administrator access was not granted; the snapshot cannot continue.</source>
        <translation>No s&apos;ha garantit l&apos;accés com a administrador; la instantània no pot continuar.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="334"/>
        <source>Detected newer exclusion file at %1 compared to %2. Prompting for action.</source>
        <translation>S&apos;ha detectat un fitxer d&apos;exclusió més nou a %1 en comparació amb %2. Se sol·licita una acció.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="341"/>
        <source>s</source>
        <comment>CLI excludes prompt: single-letter shortcut for 'show diff'</comment>
        <translation>s</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="342"/>
        <source>u</source>
        <comment>CLI excludes prompt: single-letter shortcut for 'use updated default'</comment>
        <translation>u</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="343"/>
        <source>k</source>
        <comment>CLI excludes prompt: single-letter shortcut for 'keep custom (update timestamp)'</comment>
        <translation>k</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="344"/>
        <source>q</source>
        <comment>CLI excludes prompt: single-letter shortcut for 'quit'</comment>
        <translation>q</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="346"/>
        <source>show diff</source>
        <comment>CLI excludes prompt option label</comment>
        <translation>Mostra les diferències</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="347"/>
        <source>use updated default</source>
        <comment>CLI excludes prompt option label</comment>
        <translation>Usa el fitxer predeterminat actualitzat</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="348"/>
        <source>keep custom (update timestamp)</source>
        <comment>CLI excludes prompt option label</comment>
        <translation>Mantén el personalitzat (actualitza la marca de temps)</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="349"/>
        <source>quit</source>
        <comment>CLI excludes prompt option label</comment>
        <translation>Surt</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="352"/>
        <source>[%1]%2  [%3]%4  [%5]%6  [%7]%8: </source>
        <translation>[%1]%2  [%3]%4  [%5]%6  [%7]%8: </translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="357"/>
        <source>The exclusion file at %1 is newer than your configured file at %2.</source>
        <translation>El fitxer d&apos;exclusió a %1 és més nou que el fitxer configurat a %2.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="373"/>
        <source>Reverted to updated default exclusion file.</source>
        <translation>S&apos;ha revertit al fitxer actualitzat d&apos;exclusió per defecte.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="390"/>
        <source>No input available to answer the exclusion file prompt; aborting without creating a snapshot.</source>
        <translation>No hi ha resposta a la pregunta del fitxer d&apos;exclusions; s&apos;interromp sense crear la instantània.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="393"/>
        <source>Leaving custom exclusion file unchanged.</source>
        <translation>Es deixa el fitxer d&apos;exclusió personalitzat sense canvis.</translation>
    </message>
    <message>
        <location filename="../src/batchprocessing.cpp" line="404"/>
        <source>Invalid choice. Please select again.</source>
        <translation>Opció no vàlida. Si us plau, seleccioneu un altre cop.</translation>
    </message>
</context>
<context>
    <name>Cmd</name>
    <message>
        <location filename="../src/cmd.cpp" line="121"/>
        <source>No elevation tool found (pkexec/gksu/sudo).</source>
        <translation>No s&apos;ha trobat cap eina d&apos;elevació (pkexec / gksu / sudo).</translation>
    </message>
    <message>
        <location filename="../src/cmd.cpp" line="217"/>
        <source>Failed to start command: %1</source>
        <translation>Ha fallat en iniciar l&apos;ordre: %1</translation>
    </message>
    <message>
        <location filename="../src/cmd.cpp" line="288"/>
        <source>Administrator access was not granted (authentication cancelled or denied).</source>
        <translation>No s&apos;ha garantit l&apos;accés com a administrador (autenticació cancel·lada o denegada).</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <location filename="../src/mainwindow.ui" line="14"/>
        <location filename="../src/mainwindow.cpp" line="401"/>
        <location filename="../src/mainwindow.cpp" line="1023"/>
        <source>MX Snapshot</source>
        <translation>MX Snapshot </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="33"/>
        <source>Optional customization</source>
        <translation>Personalització opcional</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="65"/>
        <source>Release version:</source>
        <translation>Versió de l&apos;edició:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="79"/>
        <source>Boot options:</source>
        <translation>Opcions d&apos;arrencada:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="86"/>
        <source>Live kernel:</source>
        <translation>Kernel autònom:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="93"/>
        <source>Project name:</source>
        <translation>Nom de projecte:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="100"/>
        <source>Release date:</source>
        <translation>Data de publicació:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="107"/>
        <source>Release codename:</source>
        <translation>Nom en clau de l&apos;edició:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="133"/>
        <source>Current date</source>
        <translation>Data actual</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="172"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Snapshot is a utility that creates a bootable image (ISO) of your working system that you can use for storage or distribution. You can continue working with undemanding applications while it is running.&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Snapshot és una eina que crea una imatge autònoma (ISO) del vostre sistema en funcionament, que podeu usar per a emmagatzematge o distribució. Podeu continuar usant aplicacions poc intensives mentre s&apos;executa.&lt;br/&gt;&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="182"/>
        <source>Used space on / (root) and /home partitions:</source>
        <translation>Espai usat a les particions / (arrel) i /home: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="192"/>
        <source>Location and ISO name</source>
        <translation>Ubicació i nom de l&apos;ISO</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="219"/>
        <source>Snapshot location:</source>
        <translation>Ubicació de la instantània:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="226"/>
        <source>Select a different snapshot directory</source>
        <translation>Trieu un directori diferent per a la instantània </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="236"/>
        <location filename="../src/mainwindow.cpp" line="669"/>
        <source>Snapshot name:</source>
        <translation>Nom de la instantània: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="360"/>
        <source>Type of snapshot:</source>
        <translation>Tipus d&apos;instantània: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="367"/>
        <source>Preserving accounts (for personal backup)</source>
        <translation>Preservant els comptes (còpia de seguretat personal) </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="377"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;This option will reset &amp;quot;demo&amp;quot; and &amp;quot;root&amp;quot; passwords to the MX Linux defaults and will not copy any personal accounts created.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Aquesta opció inicialitzarà les contrasenyes per &amp;quot;demo&amp;quot; i &amp;quot;root&amp;quot; als valors per omissió de MX Linux i no copiarà cap compte personal que s&apos;hagi creat.&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="380"/>
        <source>Resetting accounts (for distribution to others)</source>
        <translation>Es restableixen els comptes (per distribuir a tercers) </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="451"/>
        <source>You can also exclude certain directories by ticking the common choices below, or by clicking on the button to directly edit /etc/mx-snapshot-exclude.list.</source>
        <translation>També podeu excloure alguns directoris marcant les opcions comunes a sota, o clicant el botó per editar directament /etc/mx-snapshot-exclude.list.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="519"/>
        <source>Throttle the I/O input rate by the given percentage.</source>
        <translation>Ajusta el règim d&apos;E/S amb el percentatge donat.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="525"/>
        <source>I/O throttle:</source>
        <translation>Ajust d&apos;E/S:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="532"/>
        <source>Calculate checksums:</source>
        <translation>Calcula les sumes de control:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="539"/>
        <source>ISO compression scheme:</source>
        <translation>Mètode de compressió de l&apos;ISO:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="546"/>
        <source>Number of CPU cores to use:</source>
        <translation>Nombre de nuclis de CPU a usar:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="553"/>
        <source>md5</source>
        <translation>md5</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="563"/>
        <source>Options:</source>
        <translation>Opcions:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="610"/>
        <source>Edit Exclusion File</source>
        <translation>Edita el fitxer d&apos;exclusions</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="626"/>
        <location filename="../src/mainwindow.cpp" line="1048"/>
        <source>Remove Custom Exclusion File</source>
        <translation>Elimina el fitxer d&apos;exclusions personalitzat</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="679"/>
        <source>Pictures</source>
        <translation>Imatges</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="686"/>
        <source>Videos</source>
        <translation>Vídeos</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="693"/>
        <source>All of the above</source>
        <translation>Tots els esmentats</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="700"/>
        <source>exclude network configurations</source>
        <translation>exclou configuracions de xarxes</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="703"/>
        <source>Networks</source>
        <translation>Xarxes</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="710"/>
        <source>Downloads</source>
        <translation>Baixades </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="717"/>
        <source>Desktop</source>
        <translation>Escriptori</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="724"/>
        <source>Documents</source>
        <translation>Documents</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="731"/>
        <source>Flatpaks</source>
        <translation>Flatpaks</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="738"/>
        <source>Music</source>
        <translation>Música</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="881"/>
        <source>About this application</source>
        <translation>Quant a aquest programa</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="884"/>
        <source>About...</source>
        <translation>Quant a...</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="890"/>
        <source>Alt+B</source>
        <translation>Alt+B</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="1005"/>
        <source>Next</source>
        <translation>Següent</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="906"/>
        <source>Display help </source>
        <translation>Mostra l&apos;ajuda</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="909"/>
        <source>Help</source>
        <translation>Ajuda </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="915"/>
        <source>Alt+H</source>
        <translation>Alt+H</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="1030"/>
        <source>Quit application</source>
        <translation>Surt de l&apos;aplicació</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="1033"/>
        <source>Cancel</source>
        <translation>Cancel·la</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="1039"/>
        <source>Alt+N</source>
        <translation>Alt+N</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="989"/>
        <source>Back</source>
        <translation>Enrere</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="363"/>
        <source>Select Release Date</source>
        <translation>Trieu la data de llançament</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="407"/>
        <source>fastest, worst compression</source>
        <translation>més ràpid, pitjor compressió</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="407"/>
        <source>fast, worse compression</source>
        <translation>ràpid, pitjor compressió</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="408"/>
        <source>slow, better compression</source>
        <translation>lent, millor compressió</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="408"/>
        <source>best compromise</source>
        <translation>millor compromís</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="409"/>
        <source>slowest, best compression</source>
        <translation>més lent, millor compressió</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="438"/>
        <source>Free space on %1, where snapshot folder is placed: </source>
        <translation>Espai lliure a %1, on es desarà la carpeta de la instantània: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="441"/>
        <source>The free space should be sufficient to hold the compressed data from / and /home

      If necessary, you can create more available space by removing previous snapshots and saved copies: %1 snapshots are taking up %2 of disk space.</source>
        <translation>Hauria d&apos;haver-hi prou espai lliure per allotjar les dades comprimides de / i /home.    Si cal, podeu crear més espai lliure disponible per allotjar les instantànies anteriors i les còpies desades: per %1 instantànies cal fins a %2 d&apos;espai de disc.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="453"/>
        <location filename="../src/mainwindow.cpp" line="454"/>
        <source>Installing </source>
        <translation>S&apos;instal·la </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="601"/>
        <source>Please wait.</source>
        <translation>Espereu, si us plau. </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="603"/>
        <source>Please wait. Calculating used disk space...</source>
        <translation>Espereu, si us plau. Calculant l&apos;espai de disc usat...</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="280"/>
        <location filename="../src/mainwindow.cpp" line="503"/>
        <location filename="../src/mainwindow.cpp" line="619"/>
        <location filename="../src/mainwindow.cpp" line="648"/>
        <location filename="../src/mainwindow.cpp" line="655"/>
        <location filename="../src/mainwindow.cpp" line="738"/>
        <location filename="../src/mainwindow.cpp" line="749"/>
        <location filename="../src/mainwindow.cpp" line="789"/>
        <location filename="../src/mainwindow.cpp" line="1113"/>
        <source>Error</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="180"/>
        <location filename="../src/mainwindow.cpp" line="216"/>
        <source>Updated Exclusion List</source>
        <translation>Llista d&apos;exclusió actualitzada</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="183"/>
        <source>The exclusion file at %1 is newer than your configured file at %2.</source>
        <translation>El fitxer d&apos;exclusió a %1 és més nou que el fitxer configurat a %2.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="185"/>
        <source>Review the changes below. Keep your custom file or replace it with the updated default.</source>
        <translation>Reviseu els canvis següents. Conserveu el fitxer personalitzat o substituïu-lo pel predeterminat actualitzat.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="191"/>
        <source>Show Differences</source>
        <translation>Mostra&apos;n les diferències</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="197"/>
        <source>Keep Custom</source>
        <translation>Mantén el fitxer personalitzat</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="203"/>
        <source>Use Updated Default</source>
        <translation>Usa el fitxer predeterminat actualitzat</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="504"/>
        <source>Administrator access was not granted; the snapshot cannot continue.</source>
        <translation>No s&apos;ha garantit l&apos;accés com a administrador; la instantània no pot continuar.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="536"/>
        <source>Done</source>
        <translation>Fet </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="649"/>
        <source>Output file %1 already exists. Please use another file name, or delete the existent file.</source>
        <translation>El fitxer de sortida %1 ja hi és. Si us plau, useu un altre nom de fitxer, o esborreu el fitxer existent.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="656"/>
        <source>Insufficient free space. Please select a different snapshot directory or free up space.</source>
        <translation>Espai lliure insuficient. Seleccioneu un directori d&apos;instantànies diferent o allibereu espai.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="660"/>
        <source>Settings</source>
        <translation>Paràmetres</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="666"/>
        <source>Snapshot will use the following settings:</source>
        <translation>La instantània usarà aquests paràmetres:</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="668"/>
        <source>- Snapshot directory:</source>
        <translation>- Directori de la instantània: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="669"/>
        <source>- Kernel to be used:</source>
        <translation>- Kernel a usar: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="694"/>
        <location filename="../src/mainwindow.cpp" line="703"/>
        <source>NVIDIA Detected</source>
        <translation>Detectada NVIDIA</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="695"/>
        <source>This computer uses an NVIDIA graphics card. Are you planning to use the resulting ISO on the same computer or another computer with an NVIDIA card?</source>
        <translation>Aquest ordinador usa una targeta gràfica NVIDIA. Teniu previst usar l&apos;ISO obtingut en el mateix ordinador o un altre amb targeta NVIDIA?</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="699"/>
        <source>NVIDIA Selected</source>
        <translation>Seleccionada NVIDIA</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="700"/>
        <source>Note: If you use the resulting ISO on a computer without an NVIDIA card, you will likely need to remove &apos;xorg=nvidia&apos; from the boot options.</source>
        <translation>Nota: Si voleu usar l&apos;ISO obtingut en un ordinador sense targeta NVIDIA, probablement necessitareu eliminar &apos;xorg=nvidia&apos; de les opcions d&apos;arrencada.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="704"/>
        <source>Note: If you use the resulting ISO on a computer with an NVIDIA card, you may need to add &apos;xorg=nvidia&apos; to the boot options.</source>
        <translation>Nota: Si useu l&apos;ISO obtingut en on ordinador amb targeta NVIDIA, probablement necessitareu afegir &apos;xorg=nvidia&apos; a les opcions d&apos;arrencada.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="281"/>
        <location filename="../src/mainwindow.cpp" line="739"/>
        <source>Could not replace the exclusion file with the updated default.</source>
        <translation>No s&apos;ha pogut substituir el fitxer d&apos;exclusió pel valor predeterminat actualitzat.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="750"/>
        <source>Current kernel doesn&apos;t support selected compression algorithm, please edit the configuration file and select a different algorithm.</source>
        <translation>El kernel actual no dona suport a l&apos;algoritme de compressió seleccionat, si us plau editeu el fitxer de configuració i trieu un algoritme diferent.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="785"/>
        <source>Cancelled</source>
        <translation>Cancel·lat</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="786"/>
        <source>Administrator access is required to create a snapshot.</source>
        <translation>Cal accés com a administrador per crear una instantània.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="787"/>
        <source>The operation was cancelled. Select Next when you are ready to try again.</source>
        <translation>S&apos;ha cancel·lat l&apos;operació. Trieu Següent quan estigueu a pint de tornar-ho a provar.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="790"/>
        <source>Could not create the snapshot or work directory.</source>
        <translation>No s&apos;ha pogut crear la instantània o el directori de treball. </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="816"/>
        <source>These services in /etc/systemd/system contain what look like passwords, tokens or keys, and will be included in the snapshot as they are:</source>
        <translation>Aquests serveis a /etc/systemd/system contenen el que sembla contrasenyes, testimonis o claus i s&apos;inclouran a la instantània tal com estan.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="822"/>
        <source>These service files or folders could not be checked for passwords, tokens or keys, and will be included in the snapshot as they are:</source>
        <translation>Aquests fitxers de serveis o directoris no s&apos;ha pogut comprovar si contenen contrasenyes, testimonis o claus i s&apos;inclouran a la instantània tal com estan.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="826"/>
        <source>Every system started or installed from this ISO will have them. If the ISO will be shared, remove the credentials from these files or add the files to the exclusion list. Disabling a service is not enough: its files are still copied.</source>
        <translation>Cada sistema arrencat o instal·lat des d&apos;aquest ISO els tindrà. Si cal compartir la ISO, tragueu les credencials d&apos;aquests fitxers o afegiu els fitxers a la llista d&apos;exclusió. Si desactiveu un servei no és suficient: els fitxers es copiaran igualment.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="829"/>
        <source>Continue anyway?</source>
        <translation>Continua igualment?</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="831"/>
        <source>Credentials in services</source>
        <translation>Credencials als serveis</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="839"/>
        <source>Final chance</source>
        <translation>Darrera oportunitat </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="841"/>
        <source>Snapshot now has all the information it needs to create an ISO from your running system.</source>
        <translation>La instantània ara té tota la informació necessària per crear un ISO a partir del vostre sistema en execució. </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="842"/>
        <source>It will take some time to finish, depending on the size of the installed system and the capacity of your computer.</source>
        <translation>Cal un cert temps per acabar, depenent de la mida del vostre sistema instal·lat i de la potència de l&apos;ordinador. </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="844"/>
        <source>OK to start?</source>
        <translation>D&apos;acord per començar?</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.ui" line="824"/>
        <location filename="../src/mainwindow.cpp" line="848"/>
        <source>Shutdown computer when done.</source>
        <translation>Tanca l&apos;ordinador en acabar.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="884"/>
        <source>Output</source>
        <translation>Sortida </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="928"/>
        <source>Close</source>
        <translation>Tanca</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1003"/>
        <source>Edit Boot Menu</source>
        <translation>Edita el Menú d&apos;Arrencada </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1004"/>
        <source>The program will now pause to allow you to edit any files in the work directory. Select Yes to edit the boot menu or select No to bypass this step and continue creating the snapshot.</source>
        <translation>El programa ara farà una pausa per permetre-us editar fitxers al directori de treball. Trieu Sí per editar el menú d&apos;arrencada, o No per saltar aquest pas i continuar creant la instantània. </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1049"/>
        <source>Revert the exclusion list to the default file? This will overwrite your current exclusions.</source>
        <translation>Revertir la llista d&apos;exclusions al fitxer per omissió? Això sobreescriurà les vostres exclusions actuals.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1081"/>
        <source>About %1</source>
        <translation>Quant a %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1083"/>
        <source>Version: </source>
        <translation>Versió: </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1084"/>
        <source>Program for creating a live-CD from the running system for MX Linux</source>
        <translation>Programa de MX Linux per crear un CD autònom a partir del sistema en execució </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1086"/>
        <source>Copyright (c) MX Linux</source>
        <translation>Copyright (c) MX Linux</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1088"/>
        <source>%1 License</source>
        <translation>Llicència de %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1101"/>
        <source>%1 Help</source>
        <translation>Ajuda de %1</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1106"/>
        <source>Select Snapshot Directory</source>
        <translation>Trieu el directori de la instantània </translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1114"/>
        <source>Insufficient free space in the selected directory. Please choose a different location.</source>
        <translation>No hi ha prou espai lliure al directori seleccionat. Trieu una ubicació diferent.</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1157"/>
        <source>Confirmation</source>
        <translation>Confirmació</translation>
    </message>
    <message>
        <location filename="../src/mainwindow.cpp" line="1157"/>
        <source>Are you sure you want to quit the application?</source>
        <translation>Segur que voleu sortir de l&apos;aplicació?</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../src/main.cpp" line="122"/>
        <source>Tool used for creating a live-CD from the running system</source>
        <translation>Eina per crear CD autònoms a partir del sistema en execució</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="126"/>
        <source>Use CLI only</source>
        <translation>Usa només CLI</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="130"/>
        <source>Number of CPU cores to be used.</source>
        <translation>Nombre de nuclis d&apos;E/S que s&apos;usaran.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="131"/>
        <source>Output directory</source>
        <translation>Directori de sortida</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="132"/>
        <source>Output filename</source>
        <translation>Nom del fitxer de sortida</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="134"/>
        <source>Name a different kernel to use other than the default running kernel, use format returned by &apos;uname -r&apos;</source>
        <translation>Anomeneu un kernel diferent que l&apos;usat per omissió, useu el format que retorna l&apos;ordre &apos;uname -r&apos;</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="136"/>
        <source>Or the full path: %1</source>
        <translation>O el camí sencer: %1</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="139"/>
        <source>Compression level options.</source>
        <translation>Opcions del nivell de compressió.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="140"/>
        <source>Use quotes: &quot;-Xcompression-level &lt;level&gt;&quot;, or &quot;-Xalgorithm &lt;algorithm&gt;&quot;, or &quot;-Xhc&quot;, see mksquashfs man page</source>
        <translation>Useu cometes dobles: &quot;-Xcompression-level &lt;level&gt;&quot;, o &quot;-Xalgorithm &lt;algorithm&gt;&quot;, o &quot;-Xhc&quot;, vegeu la pàgina man de mksquashfs</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="144"/>
        <source>Create a monthly snapshot, add &apos;Month&apos; name in the ISO name, skip used space calculation</source>
        <translation>Crea una instantània mensual, afegiu el nom &apos;Month&apos; al nom de la ISO, s&apos;omet l&apos;ús del càlcul de mida</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="145"/>
        <source>This option sets reset-accounts and compression to defaults, arguments changing those items will be ignored</source>
        <translation>Aquesta opció defineix reset-accounts i compression als valors per omissió, s&apos;ignoraran els arguments que canviïn aquests paràmetres</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="147"/>
        <source>Optionally specify a suffix to add to the month name (e.g., &apos;1&apos; for &apos;July.1&apos;)</source>
        <translation>Opcionalment, especifiqueu un sufix per afegir al nom del mes (per exemple, 1 per a juliol.1)</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="148"/>
        <source>Don&apos;t calculate checksums for resulting ISO file</source>
        <translation>No calcula la suma de verificació per a l&apos;ISO resultant</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="149"/>
        <source>Skip calculating free space to see if the resulting ISO will fit</source>
        <translation>Ometre el càlcul d&apos;espai lliure per saber si el fitxer ISO resultant hi cap</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="150"/>
        <source>Option to fix issue with calculating checksums on preempt_rt kernels</source>
        <translation>Opció per esmenar el problema causat pel càlcul de les sumes de verificació als nuclis preempt_rt</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="151"/>
        <source>Resetting accounts (for distribution to others)</source>
        <translation>Es restableixen els comptes (per distribuir a tercers) </translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="152"/>
        <source>Calculate checksums for resulting ISO file</source>
        <translation>Calcula la suma de verificació per a l&apos;ISO resultant</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="154"/>
        <source>Throttle the I/O input rate by the given percentage. This can be used to reduce the I/O and CPU consumption of Mksquashfs.</source>
        <translation>Ajusta el règim d&apos;entrada d&apos;E/S amb el percentatge donat. Es pot usar per reduir l&apos;E/S i el consum de CPU de Mksquashfs.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="157"/>
        <source>Work directory</source>
        <translation>Directori de treball</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="159"/>
        <source>Exclude main folders, valid choices: </source>
        <translation>Exclou les carpetes principals, opcions vàlides: </translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="161"/>
        <source>Use the option one time for each item you want to exclude</source>
        <translation>Useu l&apos;opció un cop per cada ítem que vulgueu excloure</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="164"/>
        <source>Compression format, valid choices: </source>
        <translation>Format de compressió, opcions vàlides: </translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="166"/>
        <source>Shutdown computer when done.</source>
        <translation>Tanca l&apos;ordinador en acabar.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="167"/>
        <source>Use grub for legacy mbr iso boot instead of isolinux/syslinux.</source>
        <translation>Useu GRUB per l&apos;arrencada clàssica MBR en comptes de isolinux/syslinux.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="168"/>
        <source>Add the &apos;xorg=nvidia&apos; boot option to the ISO (for booting on a system with an NVIDIA card).</source>
        <translation>Afegiu l&apos;opció d&apos;arrencada &apos;xorg=nvidia&apos; a la ISO (per arrencar en un sistema amb targeta nVidia).</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="176"/>
        <source>Optional suffix for a monthly snapshot; only valid together with --month.</source>
        <translation>Sufix opcional per a d&apos;instantània mensual; només vàlid juntament amb --month.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="209"/>
        <source>A single suffix positional argument is only valid together with --month.</source>
        <translation>Un argument posicional amb un sol sufix només és vàlid juntament amb --month.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="273"/>
        <source>You seem to be logged in as root, please log out and log in as normal user to use this program.</source>
        <translation>Sembla que esteu connectat com a administrador, si us plau sortiu i connecteu-vos com a usuari normal per usar aquest programa.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="290"/>
        <source>version:</source>
        <translation>versió:</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="284"/>
        <source>You must run this program with sudo or pkexec.</source>
        <translation>Cal executar aquest programa amb sudo o pkexec.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="248"/>
        <source>MX Snapshot</source>
        <translation>MX Snapshot </translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="275"/>
        <location filename="../src/main.cpp" line="354"/>
        <location filename="../src/main.cpp" line="360"/>
        <location filename="../src/main.cpp" line="400"/>
        <location filename="../src/settings.cpp" line="796"/>
        <location filename="../src/settings.cpp" line="805"/>
        <location filename="../src/settings.cpp" line="1445"/>
        <location filename="../src/settings.cpp" line="1555"/>
        <source>Error</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="299"/>
        <source>Cancelled before the snapshot started.</source>
        <translation>Cancel·lat abans d&apos;engegar la instantània.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="328"/>
        <source>Fatal error:</source>
        <translation>Error fatal:</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="330"/>
        <source>Fatal error: unknown exception</source>
        <translation>Error fatal: excepció desconeguda</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="355"/>
        <source>Another snapshot is already running (MX Snapshot or iso-snapshot-cli). Wait for it to finish, then try again.</source>
        <translation>Ja està funcionant una altra instantània (MX Snapshot o iso-snapshot-cli). Espereu a que acabi, llavors torneu a provar-ho.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="361"/>
        <source>Cannot use the lock file %1 (%2). It keeps two snapshots from running at once. Restarting the computer clears it, or an administrator can delete the file.</source>
        <translation>No es pot usar el fitxer de bloqueig %1 (%2). Té dues instantànies executant-se alhora. Reiniciar l&apos;ordinador ho neteja, o bé un administrador pot eliminar el fitxer.</translation>
    </message>
    <message>
        <location filename="../src/main.cpp" line="399"/>
        <location filename="../src/settings.cpp" line="804"/>
        <source>Current kernel doesn&apos;t support Squashfs, cannot continue.</source>
        <translation>El kernel actual no dona suport a Squashfs, no puc continuar.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="274"/>
        <source>Exception during initialization: %1</source>
        <translation>Excepció durant la inicialització: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="229"/>
        <source>Pending Arch bind-root cleanup state but installed-to-live-arch is missing: %1</source>
        <translation>Mentre l&apos;estat de neteja d&apos;Arch bind-root, però hi manca installed-to-live-arch: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="277"/>
        <source>Unknown exception during initialization</source>
        <translation>Excepció desconeguda durant la inicialització</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="312"/>
        <source>Could not create work directory. </source>
        <translation>No s&apos;ha pogut crear el directori de treball.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="395"/>
        <source>No suitable filesystem found for the temp directory. Tried /tmp, /home, and the snapshot directory.</source>
        <translation>No trobo un sistema de fitxers adequat per al directori temp. He provat /tmp, /home, i el directori de la instantània.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="406"/>
        <source>Could not create temp directory:</source>
        <translation>No s&apos;ha pogut crear el directori temporal:</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="407"/>
        <source>Please check that the parent directory exists and is writable:</source>
        <translation>Si us plau, comproveu que el directori principal existeixi i que s&apos;hi pugui escriure:</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="425"/>
        <source>Compression format &apos;%1&apos; is not supported by the current kernel</source>
        <translation>El format de compressió %1 no és compatible amb el nucli actual.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="431"/>
        <source>Invalid cores setting: %1. Must be between 1 and %2</source>
        <translation>Configuració de nuclis no vàlida: %1. Ha de ser entre 1 i %2.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="440"/>
        <source>Invalid throttle setting: %1. Must be between 0 and 99</source>
        <translation>Valor de l&apos;accelerador no vàlid: %1. Ha d&apos;estar entre 0 i 99</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="446"/>
        <source>Snapshot directory cannot be empty</source>
        <translation>El directori d&apos;instantànies no pot estar buit.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="503"/>
        <source>Snapshot name cannot be empty</source>
        <translation>El nom de la instantània no pot estar buit.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="507"/>
        <source>Snapshot name contains invalid characters: %1</source>
        <translation>El nom de la instantània conté caràcters no vàlids: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="459"/>
        <source>Kernel version cannot be empty</source>
        <translation>La versió del nucli no pot estar buida.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="464"/>
        <source>Kernel file not found: /boot/vmlinuz-%1</source>
        <translation>No s&apos;ha trobat el fitxer del nucli: /boot/vmlinuz-%1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="471"/>
        <source>Kernel %1 doesn&apos;t support Squashfs</source>
        <translation>El nucli %1 no admet Squashfs.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="518"/>
        <source>Exclusion file does not exist: %1</source>
        <translation>El fitxer d&apos;exclusió no existeix: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="527"/>
        <source>Unbalanced quotes in exclusion list</source>
        <translation>Cites desequilibrades a la llista d&apos;exclusions</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="556"/>
        <source>Insufficient free space: %1 KiB available, minimum %2 KiB required</source>
        <translation>Espai lliure insuficient: %1 KiB disponible, el mínim requerit és de %2 KiB.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="563"/>
        <source>Insufficient free space in work directory: %1 KiB available, minimum %2 KiB required</source>
        <translation>Espai lliure insuficient al directori de treball: %1 KiB disponible, el mínim requerit és de %2 KiB.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="578"/>
        <source>Failed to determine number of CPU cores</source>
        <translation>No s&apos;ha pogut determinar el nombre de nuclis de CPU.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="584"/>
        <source>Configuration file does not exist: %1</source>
        <translation>El fitxer de configuració no existeix: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="585"/>
        <source>Using default settings</source>
        <translation>S&apos;usa la configuració predeterminada.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="587"/>
        <source>Cannot read configuration file: %1</source>
        <translation>No es pot llegir el fitxer de configuració: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="588"/>
        <source>Error: %1</source>
        <translation>Error: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="598"/>
        <source>Required tool not found: %1</source>
        <translation>No s&apos;ha trobat l&apos;eina necessària: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="607"/>
        <source>Required directory not found: %1</source>
        <translation>No s&apos;ha trobat el directori requerit: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="626"/>
        <source>Initialization Error</source>
        <translation>Error d&apos;inicialització</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="627"/>
        <source>Failed to initialize application settings:

%1</source>
        <translation>No s&apos;ha pogut iniciar la configuració de l&apos;aplicació:

%1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="794"/>
        <source>Could not find a usable kernel</source>
        <translation>No he trobat un kernel utilitzable</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="795"/>
        <source>Searched for kernel files in /boot/ but none were found or accessible.</source>
        <translation>S&apos;han cercat fitxers del nucli a /boot/, però no s&apos;han trobat ni s&apos;hi pot accedir.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="817"/>
        <source>No users found in the system</source>
        <translation>No s&apos;han trobat usuaris al sistema.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="820"/>
        <source>Failed to determine system information</source>
        <translation>No s&apos;ha pogut determinar la informació del sistema.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="980"/>
        <source>Used space on / (root): </source>
        <translation>Espai usat a / (arrel): </translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="996"/>
        <source>estimated</source>
        <translation>estimat</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1003"/>
        <source>Used space on /home: </source>
        <translation>Espai usat a /home: </translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1064"/>
        <source>Free space on %1, where snapshot folder is placed: </source>
        <translation>Espai lliure a %1, on es desarà la instantània: </translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1068"/>
        <source>The free space should be sufficient to hold the compressed data from / and /home

      If necessary, you can create more available space
      by removing previous snapshots and saved copies:
      %1 snapshots are taking up %2 of disk space.
</source>
        <translation>L&apos;espai lliure hauria de ser suficient per encabir-hi les dades comprimides de / i /home.      Si cal, podeu disposar de més espai si suprimiu instantànies      anteriors i còpies desades:       %1 instantànies usen fins a %2 d&apos;espai de disc.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1215"/>
        <source>Error reading system configuration file: %1</source>
        <translation>Error en llegir el fitxer de configuració del sistema: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1224"/>
        <source>Error accessing user configuration</source>
        <translation>Error en accedir a la configuració de l&apos;usuari</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1256"/>
        <source>Could not create user configuration directory: %1</source>
        <translation>No puc crear el directori de configuració d&apos;usuari: %1</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1277"/>
        <source>Could not copy exclusion file from %1 to %2</source>
        <translation>No puc copiar el fitxer d&apos;exclusions de %1 a %2</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1300"/>
        <source>Unsupported compression &apos;%1&apos; in configuration, using zstd.</source>
        <translation>Compressió &apos;%1&apos; a la configuració no suportada, s&apos;usarà zstd.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1310"/>
        <source>Invalid stored cores setting (%1). Using detected CPU count: %2</source>
        <translation>Paràmetre de nuclis desat no vàlid (%1). Usant el compte de CPUs detectat, compte: %2</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1320"/>
        <source>Invalid stored throttle setting (%1). Using 0.</source>
        <translation>Valor desat de l&apos;accelerador no vàlid (%1). S&apos;usarà 0.</translation>
    </message>
    <message>
        <location filename="../src/settings.cpp" line="1443"/>
        <location filename="../src/settings.cpp" line="1553"/>
        <source>Output file %1 already exists. Please use another file name, or delete the existent file.</source>
        <translation>El fitxer de sortida %1 ja hi és. Si us plau, useu un altre nom de fitxer, o esborreu el fitxer existent.</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="73"/>
        <source>Could not load %1</source>
        <translation>No s&apos;ha pogut carregar %1</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="96"/>
        <source>License</source>
        <translation>Llicència</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="97"/>
        <location filename="../src/about.cpp" line="107"/>
        <source>Changelog</source>
        <translation>Registre de canvis</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="98"/>
        <source>Cancel</source>
        <translation>Cancel·la</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="119"/>
        <source>Could not load changelog.</source>
        <translation>No s&apos;ha pogut carregar el registre de canvis.</translation>
    </message>
    <message>
        <location filename="../src/about.cpp" line="53"/>
        <location filename="../src/about.cpp" line="122"/>
        <source>&amp;Close</source>
        <translation>Tan&amp;ca </translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="78"/>
        <source>No diff output available.</source>
        <translation>No hi ha sortida de diferències disponible.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="91"/>
        <source>Could not open default exclusion file at %1.</source>
        <translation>No puc obrir el fitxer d&apos;exclusió per omissió a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="97"/>
        <source>Could not create exclusion file directory at %1.</source>
        <translation>No uc crear el directori dels fitxers d&apos;exclusió a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="106"/>
        <source>Could not prepare exclusion file at %1.</source>
        <translation>No puc preparar el fitxer d&apos;exclusió a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="123"/>
        <source>Could not read default exclusion file at %1.</source>
        <translation>No puc llegir el fitxer d&apos;exclusió per omissió a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="131"/>
        <source>Could not write exclusion file at %1.</source>
        <translation>No puc escriure el fitxer d&apos;exclusió a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="113"/>
        <source>Could not backup existing exclusion file to %1.</source>
        <translation>No s&apos;ha pogut fer una còpia de seguretat del fitxer d&apos;exclusió existent a %1.</translation>
    </message>
    <message>
        <location filename="../src/excludesutils.cpp" line="139"/>
        <source>Could not copy default exclusion file from %1 to %2.</source>
        <translation>No puc copiar el fitxer d&apos;exclusions per omissió de %1 a %2.</translation>
    </message>
</context>
<context>
    <name>Work</name>
    <message>
        <location filename="../src/work.cpp" line="323"/>
        <source>Cleaning...</source>
        <translation>Endreçant... </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="353"/>
        <location filename="../src/work.cpp" line="1198"/>
        <source>Done</source>
        <translation>Fet </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="368"/>
        <source>Interrupted or failed to complete</source>
        <translation>Aturat o fallat en completar </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="380"/>
        <location filename="../src/work.cpp" line="436"/>
        <location filename="../src/work.cpp" line="683"/>
        <location filename="../src/work.cpp" line="691"/>
        <location filename="../src/work.cpp" line="726"/>
        <location filename="../src/work.cpp" line="775"/>
        <location filename="../src/work.cpp" line="803"/>
        <location filename="../src/work.cpp" line="817"/>
        <location filename="../src/work.cpp" line="831"/>
        <location filename="../src/work.cpp" line="839"/>
        <location filename="../src/work.cpp" line="845"/>
        <location filename="../src/work.cpp" line="853"/>
        <location filename="../src/work.cpp" line="869"/>
        <location filename="../src/work.cpp" line="908"/>
        <location filename="../src/work.cpp" line="917"/>
        <location filename="../src/work.cpp" line="932"/>
        <location filename="../src/work.cpp" line="993"/>
        <location filename="../src/work.cpp" line="1008"/>
        <location filename="../src/work.cpp" line="1016"/>
        <location filename="../src/work.cpp" line="1027"/>
        <location filename="../src/work.cpp" line="1035"/>
        <location filename="../src/work.cpp" line="1151"/>
        <location filename="../src/work.cpp" line="1168"/>
        <location filename="../src/work.cpp" line="1181"/>
        <location filename="../src/work.cpp" line="1189"/>
        <location filename="../src/work.cpp" line="1250"/>
        <location filename="../src/work.cpp" line="1572"/>
        <location filename="../src/work.cpp" line="1580"/>
        <location filename="../src/work.cpp" line="1594"/>
        <location filename="../src/work.cpp" line="1633"/>
        <location filename="../src/work.cpp" line="1676"/>
        <location filename="../src/work.cpp" line="1691"/>
        <location filename="../src/work.cpp" line="1706"/>
        <source>Error</source>
        <translation>Error</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="381"/>
        <source>Administrator access was not granted; the snapshot cannot continue.</source>
        <translation>No s&apos;ha garantit l&apos;accés com a administrador; la instantània no pot continuar.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="437"/>
        <source>There&apos;s not enough free space on your target disk, you need at least %1</source>
        <translation>No hi ha prou espai lliure al vostre disc objectiu, us cal com a mínim %1</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="440"/>
        <source>You have %1 free space on %2</source>
        <translation>Teniu un espai lliure de %1  a %2</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="443"/>
        <source>If you are sure you have enough free space rerun the program with -o/--override-size option</source>
        <translation>Si teniu la seguretat de tenir prou espai lliure, torneu a executar el programa amb l&apos;opció -o/--override-size </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="664"/>
        <source>Copying the new-iso filesystem...</source>
        <translation>Copiant el sistema de fitxers del nou-iso: </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="683"/>
        <source>ISO template not found: </source>
        <translation>No he trobat la plantilla ISO: </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="691"/>
        <location filename="../src/work.cpp" line="817"/>
        <source>Could not extract the ISO template: </source>
        <translation>No puc extreure la plantilla del ISO: </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="719"/>
        <source>Arch ISO template is missing boot/ or efi/ directories.</source>
        <translation>La plantilla ISO Arch no té els directoris boot/ o efi/.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="722"/>
        <source>Detected boot/ or efi/ under the work directory root; the template may have been extracted to the wrong location.</source>
        <translation>Detectats boot/ o efi/ a l&apos;arrel del directori de treball; la plantilla potser s&apos;ha extret a la ubicació equivocada.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="725"/>
        <source>Template: %1</source>
        <translation>Plantilla: %1 </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="761"/>
        <source>Stale archiso initramfs detected, rebuilding...</source>
        <translation>Detectat un archiso initramfs vell, reconstruint...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="765"/>
        <source>Could not determine the kernel version of /boot/archiso.img (selected kernel is %1).</source>
        <translation>No puc determinar la versió del kernel de /boot/archiso.img (el kernel triat és %1).</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="768"/>
        <source>Found /boot/archiso.img built for kernel %1, but the selected kernel is %2.</source>
        <translation>Trobada una /boot/archiso.img feta per al kernel %1, però el kernel seleccionat és el %2.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="772"/>
        <source>Rebuilding /boot/archiso.img failed; check the mkinitcpio output in the log for errors (e.g. a missing hook). Please fix them and rebuild it manually, or remove the stale file.</source>
        <translation>Ha fallat en tornar a muntar /boot/archiso.img; comproveu la sortida de registre de mkinitcpio per errors (ex:. manca un hook). Si us plau esmeneu-ho i torneu a muntar-ho manualment, o bé elimineu el fitxer avariat.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="785"/>
        <source>No /boot/archiso.img found, attempting to create one...</source>
        <translation>No he trobat la /boot/archiso.img, intentant crear-ne una...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="790"/>
        <location filename="../src/work.cpp" line="1240"/>
        <source>Warning</source>
        <translation>Atenció </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="791"/>
        <source>Could not create /boot/archiso.img (is the &apos;archiso&apos; package installed?). Falling back to the regular initramfs — the resulting ISO will likely fail to boot (&quot;Failed to start Switch Root&quot;).</source>
        <translation>No puc crear /boot/archiso.img (està instal·lat el paquet &apos;archiso&apos;?). Retornant a l &apos;initramfs normal — El ISO resultant probablement fallarà en arrencar (&quot;Falla en iniciar Switch Root&quot;).</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="803"/>
        <source>Could not find an initramfs image to use.</source>
        <translation>No trobo una imatge  initramfs per usar.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="832"/>
        <source>--grub-mbr option specified but boot/grub/i386-pc/eltorito.img is missing from iso-template</source>
        <translation>--s&apos;ha especificat l&apos;opció grub-mbr però no hi ha boot/grub/i386-pc/eltorito.img a l&apos;iso-template</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="840"/>
        <source>Could not copy the template initrd: </source>
        <translation>No puc copiar la plantilla initrd: </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="846"/>
        <source>Could not copy the kernel: </source>
        <translation>No puc copiar el kernel: </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="854"/>
        <location filename="../src/work.cpp" line="933"/>
        <location filename="../src/work.cpp" line="1017"/>
        <location filename="../src/work.cpp" line="1036"/>
        <source>Could not create the checksum for %1.</source>
        <translation>No puc crear la suma de verificació per %1.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="862"/>
        <source>Could not create temp directory. </source>
        <translation>No puc crear el directori temporal.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="869"/>
        <source>Could not extract the template initrd.</source>
        <translation>No puc extreure la plantilla initrd.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="909"/>
        <source>Could not copy the kernel modules or programs into the initrd.</source>
        <translation>No puct copiar els mòduls del kernel o programes dins de initrd.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="918"/>
        <source>Could not create the new initrd: %1</source>
        <translation>No puc crear el nou initrd:: %1</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="983"/>
        <source>Squashing filesystem...</source>
        <translation>Compactant el sistema de fitxers... </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="994"/>
        <source>Could not create linuxfs file, please check /var/log/%1.log</source>
        <translation>No he pogut crear el fitxer linuxfs, si us plau comproveu /var/log/%1.log</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1009"/>
        <location filename="../src/work.cpp" line="1028"/>
        <source>Could not move %1 to the ISO directory.</source>
        <translation>Mo puc moure %1 al directori ISO.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1142"/>
        <source>Creating CD/DVD image file...</source>
        <translation>Creant el fitxer d&apos;imatge CD/DVD...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1152"/>
        <source>Could not create ISO file, please check whether you have enough space on the destination partition.</source>
        <translation>No he pogut crear el fitxer ISO; si us plau, comproveu que hi hagi prou espai a la partició de destinació.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1162"/>
        <source>Making hybrid iso</source>
        <translation>Creant imatge ISO híbrida </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1169"/>
        <source>Could not make the ISO hybrid; it would not boot correctly from USB.</source>
        <translation>No puc crear l&apos;ISO híbrid; no arrencaria correctament des d&apos;un USB.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1182"/>
        <location filename="../src/work.cpp" line="1190"/>
        <source>Could not create the %1 checksum for the ISO.</source>
        <translation>No puc crear la suma de verificació %1 per a la ISO.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1203"/>
        <source>Success</source>
        <translation>Èxit</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1204"/>
        <source>MX Snapshot completed successfully!</source>
        <translation>MX Snapshot completat amb èxit!</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1205"/>
        <source>Snapshot took %1 to finish.</source>
        <translation>Snapshot ha necessitat %1 per acabar.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1206"/>
        <source>Thanks for using MX Snapshot, run MX Live USB Maker next!</source>
        <translation>Gràcies per usar MX Snapshot, executeu MX Live USB Maker ara!</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1221"/>
        <location filename="../src/work.cpp" line="1247"/>
        <source>Installing </source>
        <translation>S&apos;instal·la </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1236"/>
        <source>paru not found; cannot install %1 from the AUR.</source>
        <translation>No he trobat paru: no es pot instal·lar  %1 de l&apos;AUR.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1241"/>
        <source>Could not install %1; continuing without the installer.</source>
        <translation>No he pogut instal·lar %1; continuo sense l&apos;instal·lador.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1250"/>
        <source>Could not install </source>
        <translation>No s&apos;ha pogut instal·lar </translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1260"/>
        <source>Calculating checksum...</source>
        <translation>Calculant la suma de verificació...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1313"/>
        <source>Building new initrd...</source>
        <translation>Generant nou initrd...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1383"/>
        <source>Rebuilding initramfs with: mkinitcpio %1</source>
        <translation>Reconstruint intramfs amb mkinitcpio %1</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1572"/>
        <location filename="../src/work.cpp" line="1594"/>
        <source>Could not create the package list: </source>
        <translation>No puc crear la llista de paquets.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1581"/>
        <source>Could not create working directory. </source>
        <translation>No puc crear el directori de treball.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1634"/>
        <source>Could not prepare a safe bind-root overlay. Snapshot cannot continue.</source>
        <translation>No s&apos;ha pogut preparar una superposició d&apos;arrel de vinculació segura. La instantània no pot continuar.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1677"/>
        <location filename="../src/work.cpp" line="1707"/>
        <source>Could not prepare the snapshot bind-root environment.</source>
        <translation>No s&apos;ha pogut preparar l&apos;entorn d&apos;arrel de vinculació de la instantània.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="1692"/>
        <source>Could not remove this computer&apos;s snap store registration from the snapshot.</source>
        <translation>No puc eliminar el registre de l&apos;snap store de la instantània.</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="2058"/>
        <source>Calculating total size of excluded files...</source>
        <translation>Calculant la mida total dels fitxers exclosos...</translation>
    </message>
    <message>
        <location filename="../src/work.cpp" line="2085"/>
        <source>Calculating size of root...</source>
        <translation>Calculant la mida de l&apos;arrel...</translation>
    </message>
</context>
</TS>