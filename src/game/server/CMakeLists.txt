add_subdirectory( zp )

add_sources(
	CMakeLists.txt
	../../public/vinterface/interface.cpp
	../../public/vinterface/interface.h
	activity.h
	activitymap.h
	aflock.cpp
	agrunt.cpp
	airtank.cpp
	animating.cpp
	animation.cpp
	animation.h
	apache.cpp
	barnacle.cpp
	barney.cpp
	basemonster.h
	bigmomma.cpp
	bloater.cpp
	bmodels.cpp
	bullsquid.cpp
	buttons.cpp
	cbase.cpp
	cbase.h
	CBugfixedServer.cpp
	CBugfixedServer.h
	cdll_dll.h
	client.cpp
	client.h
	com_utils.cpp
	com_utils.h
	combat.cpp
	controller.cpp
	crowbar.cpp
	decals.h
	defaultai.cpp
	defaultai.h
	mp5.cpp
	doors.cpp
	doors.h
	effects.cpp
	effects.h
	enginecallback.h
	explode.cpp
	explode.h
	extdll.h
	flyingmonster.cpp
	flyingmonster.h
	func_break.cpp
	func_break.h
	func_tank.cpp
	game.cpp
	gamerules.cpp
	gamerules.h
	gargantua.cpp
	genericmonster.cpp
	ggrenade.cpp
	globals.cpp
	gman.cpp
	h_ai.cpp
	h_battery.cpp
	h_cine.cpp
	h_cycler.cpp
	h_export.cpp
	handgrenade.cpp
	hassassin.cpp
	headcrab.cpp
	healthkit.cpp
	hgrunt.cpp
	hornet.cpp
	hornet.h
	houndeye.cpp
	ichthyosaur.cpp
	islave.cpp
	items.cpp
	items.h
	leech.cpp
	lights.cpp
	maprules.cpp
	monsterevent.h
	monstermaker.cpp
	monsters.cpp
	monsters.h
	monsterstate.cpp
	mortar.cpp
	multiplay_gamerules.cpp
	nihilanth.cpp
	nodes.cpp
	nodes.h
	observer.cpp
	osprey.cpp
	path.cpp
	path.h
	pathcorner.cpp
	pch.h
	plane.cpp
	plane.h
	plats.cpp
	player.cpp
	player.h
	python.cpp
	rat.cpp
	roach.cpp
	satchel.cpp
	saverestore.h
	schedule.cpp
	schedule.h
	scientist.cpp
	scripted.cpp
	scripted.h
	scriptevent.h
	shotgun.cpp
	singleplay_gamerules.cpp
	skill.cpp
	skill.h
	sound.cpp
	soundent.cpp
	soundent.h
	spectator.cpp
	spectator.h
	squadmonster.cpp
	squadmonster.h
	subs.cpp
	talkmonster.cpp
	talkmonster.h
	teamplay_gamerules.cpp
	teamplay_gamerules.h
	tempmonster.cpp
	tentacle.cpp
	trains.h
	triggers.cpp
	turret.cpp
	util.cpp
	util.h
	util_shared.cpp
	vehicle.cpp
	voice_gamemgr.cpp
	voice_gamemgr.h
	weapons.cpp
	weapons.h
	world.cpp
	wpn_shared/hl_wpn_glock.cpp
	xen.cpp
	zombie.cpp
)

if( PLATFORM_WINDOWS )
	add_sources(
		hl.def
		resource.h
	)
endif()
