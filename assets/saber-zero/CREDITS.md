# Saber Zero donor assets

Sprites and sound effects by the Zashiko Mod, included with permission for non-commercial use.

The Black Zero palette is based on the MMX3 Zero Project romhack. This project
uses colour values only and distributes no ROM data from that romhack.

This project is not monetized. Do not use these assets commercially.

ROMs are never included; players provide their own Mega Man X USA and Mega Man X3 USA ROMs.

## Special-move sprites

`sprites/Special_Uppercut*.png`, `sprites/Special_RollingSlash*.png` and `sprites/Special_DownThrust*.png` are extracted
(with `tools/saber/extract_saber_specials.py`) from "Zero (X3, Expanded)", a custom
sheet on The Spriters Resource (Mega Man X Customs). Sprite edit by HansungKee; ride
armor pose by Justin3009; ride chaser pose by Blue Warrior. Original art © Capcom.

`sprites/Special_Crescent_FX.png` (the rolling slash's crescent) is an SNES-style
redraw made for this project (`tools/saber/extract_saber_crescent.py`). Its shapes are
traced from a rip of Zero's crescent slash in Capcom's PlayStation Mega Man X games.
Original art © Capcom.

## Special-move sounds

`sfx/crescent_slash.wav` (the rolling slash) was supplied by the project author. The
uppercut reuses `sfx/saber_1.ogg`, pitched up offline. Both go through the same
converter as the other Saber sounds (`tools/saber/mmx_saber_sfx_convert`).

## Boss-themed special art (credit pending)

`sprites/Special_Tornado.png` (the Storm tornado, `tools/saber/build_storm_tornado.py`)
and `sprites/Special_Enkoujin.png` (the downward thrust's flame dive,
`tools/saber/build_enkoujin_thrust.py`) and `sprites/Special_Cutter.png` (the Boomerang cutter
projectiles, `tools/saber/build_boomer_cutter.py`) and `sprites/Special_IceClone.png` (the ice clone,
`tools/saber/build_chill_clone.py`) and `sprites/Special_ThunderSlam.png` /
`sprites/Special_ThunderOrb.png` (the thunder slam and its orbs,
`tools/saber/build_spark_slam.py`, colour-reduced to fit the palette) are cut from AI-generated concept sheets made by a member of the project's Discord
community. The later boss-themed special strips come from the same set of sheets
(StormEagleSpecial, ChillPenguinSpecialProjectile1, SparkMandrillSpecial,
BoomerKuwangerSpecial, Enkoujin). The full credit line will be added by the project
author.
