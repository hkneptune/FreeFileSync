// *****************************************************************************
// * This file is part of the FreeFileSync project. It is distributed under    *
// * GNU General Public License: https://www.gnu.org/licenses/gpl-3.0          *
// * Copyright (C) Zenju (zenju AT freefilesync DOT org) - All Rights Reserved *
// *****************************************************************************

#include "darkmode.h"
#include <zen/sys_version.h>
#include <wx/settings.h>
#include "color_tools.h"
    #include <gtk/gtk.h>

using namespace zen;


bool zen::darkModeAvailable()
{

#if GTK_MAJOR_VERSION == 2
    return false;
#elif GTK_MAJOR_VERSION >= 3
    return true;
#else
#error unknown GTK version!
#endif

}


namespace
{
class SysColorsHook : public wxColorHook
{
public:

    wxColor getColor(wxSystemColour index) const override
    {
        //fix contrast e.g. Ubuntu's Adwaita-Dark theme and macOS dark mode:
        if (index == wxSYS_COLOUR_GRAYTEXT)
            return colGreyTextEnhContrast_;
#if 0
        auto colToString = [](wxColor c)
        {
            const auto& [rh, rl] = hexify(c.Red  ());
            const auto& [gh, gl] = hexify(c.Green());
            const auto& [bh, bl] = hexify(c.Blue ());
            const auto& [ah, al] = hexify(c.Alpha());
            return "#" + std::string({rh, rl, gh, gl, bh, bl, ah, al});
        };
        std::cerr << "wxSYS_COLOUR_GRAYTEXT " << colToString(wxSystemSettingsNative::GetColour(wxSYS_COLOUR_GRAYTEXT)) << "\n";
#endif
        return wxSystemSettingsNative::GetColour(index); //fallback
    }

private:
    const wxColor colGreyTextEnhContrast_ =
        enhanceContrast(wxSystemSettingsNative::GetColour(wxSYS_COLOUR_GRAYTEXT),
                        wxSystemSettingsNative::GetColour(wxSYS_COLOUR_WINDOWTEXT),
                        wxSystemSettingsNative::GetColour(wxSYS_COLOUR_WINDOW), 4.5 /*contrastRatioMin*/); //W3C recommends >= 4.5
};


std::optional<bool> globalDefaultThemeIsDark;
}


void zen::colorThemeInit(wxApp& app, ColorTheme colTheme) //throw FileError
{
    assert(!refGlobalColorHook());

    globalDefaultThemeIsDark = wxSystemSettings::GetAppearance().AreAppsDark();
    ZEN_ON_SCOPE_EXIT(if (!refGlobalColorHook()) refGlobalColorHook() = std::make_unique<SysColorsHook>()); //*after* SetAppearance() and despite errors

    //caveat: on macOS there are more themes than light/dark: https://developer.apple.com/documentation/appkit/nsappearance/name-swift.struct
    if (colTheme != ColorTheme::System && //"System" is already the default for macOS/Linux(GTK3)
        darkModeAvailable())
        changeColorTheme(colTheme); //throw FileError
}


void zen::colorThemeCleanup()
{
    assert(refGlobalColorHook());
    refGlobalColorHook().reset();
}


bool zen::equalAppearance(ColorTheme colTheme1, ColorTheme colTheme2)
{
    if (colTheme1 == ColorTheme::System) colTheme1 = *globalDefaultThemeIsDark ? ColorTheme::Dark : ColorTheme::Light;
    if (colTheme2 == ColorTheme::System) colTheme2 = *globalDefaultThemeIsDark ? ColorTheme::Dark : ColorTheme::Light;
    return colTheme1 == colTheme2;
}


void zen::changeColorTheme(ColorTheme colTheme) //throw FileError
{
    if (colTheme == ColorTheme::System) //SetAppearance(System) isn't working reliably! surprise!?
        colTheme = *globalDefaultThemeIsDark ? ColorTheme::Dark : ColorTheme::Light;

    try
    {
        ZEN_ON_SCOPE_SUCCESS(refGlobalColorHook() = std::make_unique<SysColorsHook>()); //*after* SetAppearance()
        if (wxApp::AppearanceResult rv = wxTheApp->SetAppearance(colTheme);
            rv != wxApp::AppearanceResult::Ok)
            throw SysError(formatSystemError("wxApp::SetAppearance",
                                             rv == wxApp::AppearanceResult::CannotChange ? L"CannotChange" : L"Failure", L"" /*errorMsg*/));
    }
    catch (const SysError& e) { throw FileError(_("Failed to update the color theme."), e.toString()); }
}
