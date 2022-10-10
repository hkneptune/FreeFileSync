// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "recycler.h"

    #include <gio/gio.h>
    #include "scope_guard.h"

using namespace zen;




//*INDENT-OFF*
void zen::moveToRecycleBin(const Zstring& itemPath) //throw FileError, RecycleBinUnavailable
{
    GFile* file = ::g_file_new_for_path(itemPath.c_str()); //never fails according to docu
    ZEN_ON_SCOPE_EXIT(g_object_unref(file);)

    GError* error = nullptr;
    ZEN_ON_SCOPE_EXIT(if (error) ::g_error_free(error));

    if (!::g_file_trash(file, nullptr, &error))
    {
        /*  g_file_trash() can fail with different error codes/messages when trash is unavailable:
                Debian 8 (GLib 2.42): G_IO_ERROR_NOT_SUPPORTED: Unable to find or create trash directory
                CentOS 7 (GLib 2.56): G_IO_ERROR_FAILED:        Unable to find or create trash directory for file.txt => localized! >:(
                master   (GLib 2.64): G_IO_ERROR_NOT_SUPPORTED: Trashing on system internal mounts is not supported
                https://gitlab.gnome.org/GNOME/glib/blob/master/gio/glocalfile.c#L2042                              */
        const bool trashUnavailable = error && error->domain == G_IO_ERROR &&
                                      (error->code == G_IO_ERROR_NOT_SUPPORTED ||

                                       //yes, the following is a cluster fuck, but what can you do?
                                       (error->code == G_IO_ERROR_FAILED && [&]
                                        {
                                            for (const char* msgLoc : //translations from https://gitlab.gnome.org/GNOME/glib/-/tree/main/po
                                            {
                                                "Unable to find or create trash directory for",
                                                "No s'ha pogut trobar o crear el directori de la paperera per",
                                                "Nelze nalézt nebo vytvořit složku koše pro",
                                                "Kan ikke finde eller oprette papirkurvskatalog for",
                                                "Αδύνατη η εύρεση ή δημιουργία του καταλόγου απορριμμάτων",
                                                "Unable to find or create wastebasket directory for",
                                                "Ne eblas trovi aŭ krei rubujan dosierujon",
                                                "No se pudo encontrar o crear la carpeta de la papelera para",
                                                "Prügikasti kataloogi pole võimalik leida või luua",
                                                "zakarrontziaren direktorioa aurkitu edo sortu",
                                                "Roskakori kansiota ei löydy tai sitä ei voi luoda",
                                                "Impossible de trouver ou créer le répertoire de la corbeille pour",
                                                "Non é posíbel atopar ou crear o directorio do lixo para",
                                                "Nisam mogao promijeniti putanju u mapu",
                                                "Nem található vagy nem hozható létre a Kuka könyvtár ehhez:",
                                                "Tidak bisa menemukan atau membuat direktori tong sampah bagi",
                                                "Impossibile trovare o creare la directory cestino per",
                                                "のゴミ箱ディレクトリが存在しないか作成できません",
                                                "휴지통 디렉터리를 찾을 수 없거나 만들 수 없습니다",
                                                "Nepavyko rasti ar sukurti šiukšlių aplanko",
                                                "Nevar atrast vai izveidot miskastes mapi priekš",
                                                "Tidak boleh mencari atau mencipta direktori tong sampah untuk",
                                                "Kan ikke finne eller opprette mappe for papirkurv for",
                                                "फाइल सिर्जना गर्न असफल:",
                                                "Impossible de trobar o crear lo repertòri de l'escobilhièr per",
                                                "ਲਈ ਰੱਦੀ ਡਾਇਰੈਕਟਰੀ ਲੱਭਣ ਜਾਂ ਬਣਾਉਣ ਲਈ ਅਸਮਰੱਥ",
                                                "Nie można odnaleźć lub utworzyć katalogu kosza dla",
                                                "Impossível encontrar ou criar a pasta de lixo para",
                                                "Não é possível localizar ou criar o diretório da lixeira para",
                                                "Nu se poate găsi sau crea directorul coșului de gunoi pentru",
                                                "Не удалось найти или создать каталог корзины для",
                                                "Nepodarilo sa nájsť ani vytvoriť adresár Kôš pre",
                                                "Ni mogoče najti oziroma ustvariti mape smeti za",
                                                "Не могу да нађем или направим директоријум смећа за",
                                                "Ne mogu da nađem ili napravim direktorijum smeća za",
                                                "Kunde inte hitta eller skapa papperskorgskatalog för",
                                                "için çöp dizini bulunamıyor ya da oluşturulamıyor",
                                                "Не вдалося знайти або створити каталог смітника для",
                                                "หาหรือสร้างไดเรกทอรีถังขยะสำหรับ",
                                            })
                                            if (contains(error->message, msgLoc))
                                                return true;

                                            for (const auto& [msgLoc1, msgLoc2] :
                                            {
                                                std::pair{"Papierkorb-Ordner konnte für", "nicht gefunden oder angelegt werden"},
                                                std::pair{"Kan prullenbakmap voor", "niet vinden of aanmaken"},
                                                std::pair{"无法为", "找到或创建回收站目录"},
                                                std::pair{"無法找到或建立", "的垃圾桶目錄"},
                                            })
                                            if (contains(error->message, msgLoc1) && contains(error->message, msgLoc2))
                                                return true;

                                            return false;
                                        }()));

        if (trashUnavailable)
            throw RecycleBinUnavailable(replaceCpy(_("The recycle bin is not available for %x."), L"%x", fmtPath(itemPath)),
                                        formatGlibError("g_file_trash", error));

        throw FileError(replaceCpy(_("Unable to move %x to the recycle bin."), L"%x", fmtPath(itemPath)),
                        formatGlibError("g_file_trash", error));
    }
}
//*INDENT-ON*
