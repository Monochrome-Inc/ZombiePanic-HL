// ============== Copyright (c) 2025 Monochrome Games ============== \\

#include <vgui_controls/Panel.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <vgui/IVGui.h>
#include "hud.h"
#include "parsemsg.h"
#include "cl_util.h"
#include "../../hud/ammohistory.h"
#include "music_ui.h"
#include "music_manager.h"
#include "client_vgui.h"
#include "vgui/client_viewport.h"

// vgui2::VarArgs
#include "zp/ui/workshop/WorkshopItemList.h"

DEFINE_HUD_ELEM( CMusicUI );

CMusicUI::CMusicUI()
    : vgui2::Panel( NULL, "MusicUI" )
{
	SetParent( g_pViewport );

	vgui2::HScheme scheme = vgui2::scheme()->LoadSchemeFromFile( VGUI2_ROOT_DIR "resource/ChatScheme.res", "ChatScheme" );
	SetScheme( scheme );

	SetPaintBackgroundEnabled( false );
	SetProportional( true );

	m_flDrawTime = gHUD.m_flTime;

	m_pTextTitleBG1 = new vgui2::Label( this, "textbg1", "Unknown Track" );
	m_pTextTitleBG1->SetContentAlignment( vgui2::Label::a_center );
	m_pTextTitleBG1->SetPaintBackgroundEnabled( false );

	m_pTextTitleBG2 = new vgui2::Label( this, "textbg2", "Unknown Track" );
	m_pTextTitleBG2->SetContentAlignment( vgui2::Label::a_center );
	m_pTextTitleBG2->SetPaintBackgroundEnabled( false );

	m_pTextTitle = new vgui2::Label( this, "text", "Unknown Track" );
	m_pTextTitle->SetContentAlignment( vgui2::Label::a_center );
	m_pTextTitle->SetPaintBackgroundEnabled( false );

	m_pTextTitle->SetPos( 0, 0 );
	m_pTextTitleBG1->SetPos( 1, 1 );
	m_pTextTitleBG2->SetPos( 2, 2 );

	InvalidateLayout();
}

void CMusicUI::Init()
{
	HideUI();
}

void CMusicUI::VidInit()
{
	HideUI();
}

void CMusicUI::HideUI()
{
	m_flDrawTime = gHUD.m_flTime;
}

void CMusicUI::DrawUI()
{
	m_flDrawTime = gHUD.m_flTime + 1.0;
}

void CMusicUI::ApplySchemeSettings( vgui2::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	vgui2::HFont fontNormal = pScheme->GetFont( "Default" );
	m_pTextTitle->SetFont( fontNormal );
	m_pTextTitle->SetFgColor( Color( 255, 255, 255, 0 ) );
	m_pTextTitleBG1->SetFont( fontNormal );
	m_pTextTitleBG1->SetFgColor( Color( 0, 0, 0, 0 ) );
	m_pTextTitleBG2->SetFont( fontNormal );
	m_pTextTitleBG2->SetFgColor( Color( 0, 0, 0, 0 ) );

	SetSize( 300, 30 );
}

bool CMusicUI::IsAllowedToDraw()
{
	return hud_draw.GetFloat() > 0.0f;
}

void CMusicUI::NewTrackPlaying()
{
	wchar_t *pString = g_pVGuiLocalize->Find( "ZP_UI_NowPlaying" );
	if ( !pString ) pString = L"Now Playing:";
	char szNowPlaying[128];
	g_pVGuiLocalize->ConvertUnicodeToANSI( pString, szNowPlaying, sizeof( szNowPlaying ) );
	const char *szFormatedString = vgui2::VarArgs( "%s %s", szNowPlaying, CMusicManager::GetInstance()->GetTrackName() );
	m_pTextTitle->SetColorCodedText( szFormatedString );
	m_pTextTitleBG1->SetColorCodedText( szFormatedString );
	m_pTextTitleBG2->SetColorCodedText( szFormatedString );
	m_flDrawTime = gHUD.m_flTime + 4.0;
}

void CMusicUI::Paint()
{
	if ( !IsAllowedToDraw() )
	{
		m_pTextTitle->SetFgColor( Color( 255, 255, 255, 0 ) );
		return;
	}

	m_pTextTitle->SetSize( GetWide(), GetTall() );
	m_pTextTitleBG1->SetSize( GetWide(), GetTall() );
	m_pTextTitleBG2->SetSize( GetWide(), GetTall() );

	int middle_pos = ScreenWidth / 2;
	int half_width = GetWide() / 2;

	int x = middle_pos - half_width;
	int a = m_pTextTitle->GetFgColor().a();

	if ( m_flDrawTime - gHUD.m_flTime > 0 )
		a += 5;
	else
		a -= 3;

	// Draw a nice rounded box behind the text.
	DrawBox(
		0, 0,
	    GetWide(), GetTall(),
		Color( 15, 15, 15, clamp( a, 0, 160 ) ),
		1.0f
	);

	m_pTextTitle->SetFgColor( Color( 255, 255, 255, clamp( a, 0, 255 ) ) );
	m_pTextTitleBG1->SetFgColor( Color( 0, 0, 0, clamp( a, 0, 255 ) ) );
	m_pTextTitleBG2->SetFgColor( Color( 0, 0, 0, clamp( a, 0, 255 ) ) );
	SetPos( x, 0 );
}