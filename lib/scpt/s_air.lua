-- handle the air school

NOXIOUSCLOUD_I = add_spell {
	["name"] = 	"Noxious Cloud I",
	["school"] = 	{SCHOOL_AIR},
	["level"] = 	3,
	["mana"] = 	5,
	["mana_max"] = 	5,
	["fail"] = 	20,
	["direction"] = TRUE,
	["spell"] = 	function(args)
			fire_cloud(Ind, GF_POIS, args.dir, (1 + get_level(Ind, NOXIOUSCLOUD_I, 52)), 3, 5 + get_level(Ind, NOXIOUSCLOUD_I, 14), 9, " fires a noxious cloud of")
	end,
	["info"] = 	function()
			return "dam "..(1 + get_level(Ind, NOXIOUSCLOUD_I, 52)).." rad 3 dur "..(5 + get_level(Ind, NOXIOUSCLOUD_I, 14))
	end,
	["desc"] = 	{
			"Creates a cloud of poison,",
			"the cloud will persist for some turns, damaging all monsters passing by.",
	}
}
NOXIOUSCLOUD_II = add_spell {
	["name"] = 	"Noxious Cloud II",
	["school"] = 	{SCHOOL_AIR},
	["level"] = 	18,
	["mana"] = 	20,
	["mana_max"] = 	20,
	["fail"] = 	-40,
	["direction"] = TRUE,
	["spell"] = 	function(args)
			fire_cloud(Ind, GF_POIS, args.dir, (1 + 50 + get_level(Ind, NOXIOUSCLOUD_II, 57)), 3, 5 + get_level(Ind, NOXIOUSCLOUD_I, 14), 9, " fires a noxious cloud of")
	end,
	["info"] = 	function()
			return "dam "..(1 + 50 + get_level(Ind, NOXIOUSCLOUD_II, 57)).." rad 3 dur "..(5 + get_level(Ind, NOXIOUSCLOUD_I, 14))
	end,
	["desc"] = 	{
			"Creates a cloud of poison,",
			"the cloud will persist for some turns, damaging all monsters passing by.",
	}
}
NOXIOUSCLOUD_III = add_spell {
	["name"] = 	"Noxious Cloud III",
	["school"] = 	{SCHOOL_AIR},
	["level"] = 	30,
	["mana"] = 	40,
	["mana_max"] = 	40,
	["fail"] = 	-70,
	["direction"] = TRUE,
	["spell"] = 	function(args)
			fire_cloud(Ind, GF_UNBREATH, args.dir, (1 + 91 + get_level(Ind, NOXIOUSCLOUD_III, 52)), 3, 5 + get_level(Ind, NOXIOUSCLOUD_I, 14), 9, " fires a noxious cloud of")
	end,
	["info"] = 	function()
			return "dam "..(91 + get_level(Ind, NOXIOUSCLOUD_III, 52)).." rad 3 dur "..(5 + get_level(Ind, NOXIOUSCLOUD_I, 14))
	end,
	["desc"] = 	{
			"Creates a cloud of thick gas, not just poisoning but also preventing",
			"living beings from breathing. The cloud will persist for some turns.",
	}
}

function get_lightningbolt_dam(Ind, limit_lev)
	--return 3 + get_level(Ind, LIGHTNINGBOLT, 25), 5 + get_level(Ind, LIGHTNINGBOLT, 25) - 1
	local lev

	lev = get_level(Ind, LIGHTNINGBOLT_I, 50)
	if limit_lev ~= 0 and lev > limit_lev then lev = limit_lev + (lev - limit_lev) / 3 end

	return 3 + (lev / 2), 5 + (lev / 2) - 1
end

LIGHTNINGBOLT_I = add_spell {
	["name"] = 	"Lightning Bolt I",
	["school"] = 	SCHOOL_AIR,
	["level"] = 	6,
	["mana"] = 	2,
	["mana_max"] = 	2,
	["fail"] = 	-10,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_ELEC, args.dir, damroll(get_lightningbolt_dam(Ind, 1)), " casts a lightning bolt for")
	end,
	["info"] = 	function()
			local x, y
			x, y = get_lightningbolt_dam(Ind, 1)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{
			"Conjures up a powerful lightning bolt.",
		}
}
LIGHTNINGBOLT_II = add_spell {
	["name"] = 	"Lightning Bolt II",
	["school"] = 	SCHOOL_AIR,
	["level"] = 	21,
	["mana"] = 	6,
	["mana_max"] = 	6,
	["fail"] = 	-30,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_ELEC, args.dir, damroll(get_lightningbolt_dam(Ind, 15)), " casts a lightning bolt for")
	end,
	["info"] = 	function()
			local x, y
			x, y = get_lightningbolt_dam(Ind, 15)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{
			"Conjures up a powerful lightning bolt.",
		}
}
LIGHTNINGBOLT_III = add_spell {
	["name"] = 	"Lightning Bolt III",
	["school"] = 	SCHOOL_AIR,
	["level"] = 	40,
	["mana"] = 	11,
	["mana_max"] = 	11,
	["fail"] = 	-75,
	["direction"] = TRUE,
	["ftk"] = 	1,
	["spell"] = 	function(args)
			fire_bolt(Ind, GF_ELEC, args.dir, damroll(get_lightningbolt_dam(Ind, 0)), " casts a lightning bolt for")
	end,
	["info"] = 	function()
			local x, y
			x, y = get_lightningbolt_dam(Ind, 0)
			return "dam "..x.."d"..y
	end,
	["desc"] = 	{
			"Conjures up a powerful lightning bolt.",
		}
}

AIRWINGS = add_spell {
	["name"] = 	"Wings of Winds",
	["school"] = 	{SCHOOL_AIR, SCHOOL_CONVEYANCE},
	["level"] = 	16,
	["mana"] = 	30,
	["mana_max"] = 	30,
	["fail"] = 	70,
	["spell"] = 	function()
			if get_level(Ind, AIRWINGS, 50) >= 16 then set_tim_lev(Ind, randint(10) + 5 + get_level(Ind, AIRWINGS, 25))
			else set_tim_ffall(Ind, randint(10) + 5 + get_level(Ind, AIRWINGS, 25))
			end
	end,
	["info"] = 	function()
			return "dur "..(5 + get_level(Ind, AIRWINGS, 25)).."+d10"
	end,
	["desc"] = 	{
			"Grants the power of feather falling.",
			"At level 16 it grants the power of levitation."
	}
}

INVISIBILITY = add_spell {
	["name"] = 	"Invisibility",
	["school"] = 	{SCHOOL_AIR},
	["level"] = 	30,
	["mana"] = 	35,
	["mana_max"] = 	35,
	["fail"] = 	-30,
	["spell"] = 	function()
--			if player.tim_invisibility == 0 then set_invis(Ind, randint(20) + 15 + get_level(Ind, INVISIBILITY, 50), 20 + get_level(Ind, INVISIBILITY, 50)) end
			set_invis(Ind, randint(20) + 15 + get_level(Ind, INVISIBILITY, 50), 20 + get_level(Ind, INVISIBILITY, 50))
	end,
	["info"] = 	function()
			return "dur "..(15 + get_level(Ind, INVISIBILITY, 50)).."+d20 power "..(20 + get_level(Ind, INVISIBILITY, 50))
	end,
	["desc"] = 	{
			"Grants invisibility."
	}
}

POISONBLOOD = add_spell {
	["name"] = 	"Poison Blood",
	["school"] = 	{SCHOOL_AIR},
	["level"] = 	22,
	["mana"] = 	15,
	["mana_max"] = 	15,
	["fail"] = 	70,
	["spell"] = 	function()
			set_oppose_pois(Ind, randint(30) + 25 + get_level(Ind, POISONBLOOD, 25))
			if get_level(Ind, POISONBLOOD, 50) >= 10 then set_brand(Ind, randint(30) + 25 + get_level(Ind, POISONBLOOD, 25), BRAND_POIS, 10) end
	end,
	["info"] = 	function()
			return "dur "..(25 + get_level(Ind, POISONBLOOD, 25)).."+d30"
	end,
	["desc"] = 	{
			"Grants poison resistance.",
			"At level 10 it provides poison branding to wielded weapon."
	}
}

THUNDERSTORM = add_spell {
	["name"] = 	"Thunderstorm",
	["school"] = 	{SCHOOL_AIR, SCHOOL_NATURE},
	["level"] = 	25,
	["mana"] = 	45,
	["mana_max"] = 	45,
	["fail"] = 	50,
	["spell"] = 	function()
			set_tim_thunder(Ind, randint(10) + 10 + get_level(Ind, THUNDERSTORM, 25), 5 + get_level(Ind, THUNDERSTORM, 10), 10 + get_level(Ind, THUNDERSTORM, 25))
	end,
	["info"] = 	function()
			return "dam "..(5 + get_level(Ind, THUNDERSTORM, 10)).."d"..(10 + get_level(Ind, THUNDERSTORM, 25)).." dur "..(10 + get_level(Ind, THUNDERSTORM, 25)).."+d10"
	end,
	["desc"] = 	{
			"Charges up the air around you with electricity.",
			"Each turn it will throw a thunder bolt at a random monster in sight.",
			"The thunder does 3 types of damage: One third of lightning,",
			"one third of sound and one third of light."
	}
}
