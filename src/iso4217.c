/*** iso4217.c -- currency symbols
 *
 * Copyright (C) 2008-2012 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <freundt@fresse.org>
 *
 * This file is part of unsermarkt.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#include <string.h>
#include "iso4217.h"

/* the list is pre-sorted */
const struct iso_4217_s iso_4217[] = {
	/* 0 */
        {"AED", 784, 2, "United Arab Emirates dirham" },
        {"AFN", 971, 2, "Afghani"},
        {"ALL",   8, 2, "Lek"},
        {"AMD",  51, 2, "Armenian dram"},

        {"ANG", 532, 2, "Netherlands Antillean guilder/florin"},
        {"AOA", 973, 2, "Kwanza"},
        {"ARS",  32, 2, "Argentine peso"},
        {"AUD",  36, 2, "Australian dollar"},
	/* 8 */
        {"AWG", 533, 2, "Aruban guilder"},
        {"AZN", 944, 2, "Azerbaijanian manat"},
        {"BAM", 977, 2, "Convertible marks"},
        {"BBD",  52, 2, "Barbados dollar"},

        {"BDT",  50, 2, "Bangladeshi taka"},
        {"BGN", 975, 2, "Bulgarian lev"},
        {"BHD",  48, 3, "Bahraini dinar"},
        {"BIF", 108, 0, "Burundian franc"},
	/* 16 */
        {"BMD",  60, 2, "Bermudian dollar (Bermuda dollar)"},
        {"BND",  96, 2, "Brunei dollar"},
        {"BOB",  68, 2, "Boliviano"},
        {"BOV", 984, 2, "Bolivian Mvdol (funds code)"},

        {"BRL", 986, 2, "Brazilian real"},
        {"BSD",  44, 2, "Bahamian dollar"},
        {"BTN",  64, 2, "Ngultrum"},
        {"BWP",  72, 2, "Pula"},
	/* 24 */
        {"BYR", 974, 0, "Belarussian ruble"},
        {"BZD",  84, 2, "Belize dollar"},
        {"CAD", 124, 2, "Canadian dollar"},
        {"CDF", 976, 2, "Franc Congolais"},

        {"CHE", 947, 2, "WIR euro (complementary currency)"},
        {"CHF", 756, 2, "Swiss franc"},
        {"CHW", 948, 2, "WIR franc (complementary currency)"},
        {"CLF", 990, 0, "Unidad de Fomento (funds code)"},
	/* 32 */
        {"CLP", 152, 0, "Chilean peso"},
        {"CNY", 156, 2, "Renminbi"},
        {"COP", 170, 2, "Colombian peso"},
        {"COU", 970, 2, "Unidad de Valor Real"},

        {"CRC", 188, 2, "Costa Rican colon"},
        {"CUP", 192, 2, "Cuban peso"},
        {"CVE", 132, 2, "Cape Verde escudo"},
        {"CZK", 203, 2, "Czech koruna"},
	/* 40 */
        {"DJF", 262, 0, "Djibouti franc"},
        {"DKK", 208, 2, "Danish krone"},
        {"DOP", 214, 2, "Dominican peso"},
        {"DZD",  12, 2, "Algerian dinar"},

        {"EEK", 233, 2, "Kroon"},
        {"EGP", 818, 2, "Egyptian pound"},
        {"ERN", 232, 2, "Nakfa"},
        {"ETB", 230, 2, "Ethiopian birr"},
	/* 48 */
        {"EUR", 978, 2, "Euro"},
        {"FJD", 242, 2, "Fiji dollar"},
        {"FKP", 238, 2, "Falkland Islands pound"},
        {"GBP", 826, 2, "Pound sterling"},

        {"GEL", 981, 2, "Lari"},
        {"GHS", 936, 2, "Cedi"},
        {"GIP", 292, 2, "Gibraltar pound"},
        {"GMD", 270, 2, "Dalasi"},
	/* 56 */
        {"GNF", 324, 0, "Guinea franc"},
        {"GTQ", 320, 2, "Quetzal"},
        {"GYD", 328, 2, "Guyana dollar"},
        {"HKD", 344, 2, "Hong Kong dollar"},

        {"HNL", 340, 2, "Lempira"},
        {"HRK", 191, 2, "Croatian kuna"},
        {"HTG", 332, 2, "Haiti gourde"},
        {"HUF", 348, 2, "Forint"},
	/* 64 */
        {"IDR", 360, 2, "Rupiah"},
        {"ILS", 376, 2, "Israeli new sheqel"},
        {"INR", 356, 2, "Indian rupee"},
        {"IQD", 368, 3, "Iraqi dinar"},

        {"IRR", 364, 2, "Iranian rial"},
        {"ISK", 352, 2, "Iceland krona"},
        {"JMD", 388, 2, "Jamaican dollar"},
        {"JOD", 400, 3, "Jordanian dinar"},
	/* 72 */
        {"JPY", 392, 0, "Japanese yen"},
        {"KES", 404, 2, "Kenyan shilling"},
        {"KGS", 417, 2, "Som"},
        {"KHR", 116, 2, "Riel"},

        {"KMF", 174, 0, "Comoro franc"},
        {"KPW", 408, 2, "North Korean won"},
        {"KRW", 410, 0, "South Korean won"},
        {"KWD", 414, 3, "Kuwaiti dinar"},
	/* 80 */
        {"KYD", 136, 2, "Cayman Islands dollar"},
        {"KZT", 398, 2, "Tenge"},
        {"LAK", 418, 2, "Kip"},
        {"LBP", 422, 2, "Lebanese pound"},

        {"LKR", 144, 2, "Sri Lanka rupee"},
        {"LRD", 430, 2, "Liberian dollar"},
        {"LSL", 426, 2, "Loti"},
        {"LTL", 440, 2, "Lithuanian litas"},
	/* 88 */
        {"LVL", 428, 2, "Latvian lats"},
        {"LYD", 434, 3, "Libyan dinar"},
        {"MAD", 504, 2, "Moroccan dirham"},
        {"MDL", 498, 2, "Moldovan leu"},

        {"MGA", 969, 7, "Malagasy ariary"},
        {"MKD", 807, 2, "Denar"},
        {"MMK", 104, 2, "Kyat"},
        {"MNT", 496, 2, "Tugrik"},
	/* 96 */
        {"MOP", 446, 2, "Pataca"},
        {"MRO", 478, 7, "Ouguiya"},
        {"MUR", 480, 2, "Mauritius rupee"},
        {"MVR", 462, 2, "Rufiyaa"},

        {"MWK", 454, 2, "Kwacha"},
        {"MXN", 484, 2, "Mexican peso"},
        {"MXV", 979, 2, "Mexican Unidad de Inversion (UDI) (funds code)"},
        {"MYR", 458, 2, "Malaysian ringgit"},
	/* 104 */
        {"MZN", 943, 2, "Metical"},
        {"NAD", 516, 2, "Namibian dollar"},
        {"NGN", 566, 2, "Naira"},
        {"NIO", 558, 2, "Cordoba oro"},

        {"NOK", 578, 2, "Norwegian krone"},
        {"NPR", 524, 2, "Nepalese rupee"},
        {"NZD", 554, 2, "New Zealand dollar"},
        {"OMR", 512, 3, "Rial Omani"},
	/* 112 */
        {"PAB", 590, 2, "Balboa"},
        {"PEN", 604, 2, "Nuevo sol"},
        {"PGK", 598, 2, "Kina"},
        {"PHP", 608, 2, "Philippine peso"},

        {"PKR", 586, 2, "Pakistan rupee"},
        {"PLN", 985, 2, "Zloty"},
        {"PYG", 600, 0, "Guarani"},
        {"QAR", 634, 2, "Qatari rial"},
	/* 120 */
        {"RON", 946, 2, "Romanian new leu"},
        {"RSD", 941, 2, "Serbian dinar"},
        {"RUB", 643, 2, "Russian ruble"},
        {"RWF", 646, 0, "Rwanda franc"},

        {"SAR", 682, 2, "Saudi riyal"},
        {"SBD",  90, 2, "Solomon Islands dollar"},
        {"SCR", 690, 2, "Seychelles rupee"},
        {"SDG", 938, 2, "Sudanese pound"},
	/* 128 */
        {"SEK", 752, 2, "Swedish krona"},
        {"SGD", 702, 2, "Singapore dollar"},
        {"SHP", 654, 2, "Saint Helena pound"},
        {"SKK", 703, 2, "Slovak koruna"},

        {"SLL", 694, 2, "Leone"},
        {"SOS", 706, 2, "Somali shilling"},
        {"SRD", 968, 2, "Surinam dollar"},
        {"STD", 678, 2, "Dobra"},
	/* 136 */
        {"SYP", 760, 2, "Syrian pound"},
        {"SZL", 748, 2, "Lilangeni"},
        {"THB", 764, 2, "Baht"},
        {"TJS", 972, 2, "Somoni"},

        {"TMM", 795, 2, "Manat"},
        {"TND", 788, 3, "Tunisian dinar"},
        {"TOP", 776, 2, "Pa'anga"},
        {"TRY", 949, 2, "New Turkish lira"},
	/* 144 */
        {"TTD", 780, 2, "Trinidad and Tobago dollar"},
        {"TWD", 901, 2, "New Taiwan dollar"},
        {"TZS", 834, 2, "Tanzanian shilling"},
        {"UAH", 980, 2, "Hryvnia"},

        {"UGX", 800, 2, "Uganda shilling"},
        {"USD", 840, 2, "US dollar"},
        {"USN", 997, 2, "United States dollar (next day) (funds code)"},
        {"USS", 998, 2, "United States dollar (same day) (funds code)"},
	/* 152 */
        {"UYU", 858, 2, "Peso"},
        {"XAG", 961, -1, "Silver (one troy ounce)"},
        {"XAU", 959, -1, "Gold (one troy ounce)"},
        {"XBA", 955, -1, "European Composite Unit (EURCO) (bond market)"},

        {"XBB", 956, -1, "European Monetary Unit (EMU-6) (bond market)"},
        {"XBC", 957, -1, "European Unit of Account 9 (EUA-9) (bond market)"},
        {"XBD", 958, -1, "European Unit of Account 17 (EUA-17) (bond market)"},
        {"XCD", 951, 2, "East Caribbean dollar"},
	/* 160 */
        {"XDR", 960, -1, "Special Drawing Rights"},
        {"XFU",   0, -1, "UIC franc (special settlement currency)"},
        {"XOF", 952, 0, "CFA Franc BCEAO"},
        {"XPD", 964, -1, "Palladium (one troy ounce)"},

        {"XPF", 953, 0, "CFP franc"},
        {"XPT", 962, -1, "Platinum (one troy ounce)"},
        {"XTS", 963, -1, "Code reserved for testing purposes"},
        {"XXX", 999, -1, "No currency"},
	/* 168 */
        {"YER", 886, 2, "Yemeni rial"},
        {"ZAR", 710, 2, "South African rand"},
        {"ZMK", 894, 2, "Kwacha"},
        {"ZWD", 716, 2, "Zimbabwe dollar"},

	/* obsolete ... kept for historical purposes */
	/* 172 */
        {"ADF",  -1, 2, "Andorran franc"},
        {"ADP",  20, 2, "Andorran peseta"},
        {"AFA",   4, -1, "Afghani"},
        {"ALK",  -1, -1, "Albanian old lek"},
	/* 176 */
        {"AON",  24, -1, "Angolan new kwanza"},
        {"AOR", 982, -1, "Angolan kwanza readjustado"},
        {"ARA",  -1, -1, "Argentine austral"},
        {"ARL",  -1, -1, "Argentine peso ley"},
	/* 180 */
        {"ARM",  -1, -1, "Argentine peso moneda nacional"},
        {"ARP",  -1, -1, "Peso argentino"},
        {"ATS",  40, 2, "Austrian schilling"},
        {"AZM",  31, -1, "Azerbaijani manat"},
	/* 184 */
        {"BEC", 993, 2, "Belgian franc (convertible)"},
        {"BEF",  56, 2, "Belgian franc (currency union with LUF)"},
        {"BEL", 992, 2, "Belgian franc (financial)"},
        {"BGJ",  -1, -1, "Bulgarian lev A/52"},
	/* 188 */
        {"BGK",  -1, -1, "Bulgarian lev A/62"},
        {"BGL", 100, -1, "Bulgarian lev A/99"},
        {"BOP",  -1, -1, "Bolivian peso"},
        {"BRB",  -1, -1, "Brazilian cruzeiro"},
	/* 192 */
        {"BRC",  -1, -1, "Brazilian cruzado"},
        {"BRE",  -1, -1, "Brazilian cruzeiro"},
        {"BRN",  -1, -1, "Brazilian cruzado novo"},
        {"BRR",  -1, -1, "Brazilian cruzeiro real"},
	/* 196 */
        {"BRY",  -1, -1, "Brazilian real"},
        {"BRZ",  -1, -1, "Brazilian cruzeiro"},
        {"CFP",  -1, -1, "Change Franc Pacifique"},
        {"CNX",  -1, -1, "Chinese People's Bank dollar"},
	/* 200 */
        {"CSD", 891, -1, "Serbian dinar"},
        {"CSJ",  -1, 2, "Czechoslovak koruna A/53"},
        {"CSK", 200, 2, "Czechoslovak koruna"},
        {"CSK", 200, 2, "Czechoslovak koruna"},
	/* 204 */
        {"CYP", 196, -1, "Cypriot pound"},
        {"DDM", 278, 2, "East German Mark of the GDR"},
        {"DEM", 276, 2, "German mark"},
        {"ECS", 218, -1, "Ecuador sucre"},
	/* 208 */
        {"ECV", 983, -1, "Ecuador Unidad de Valor Constante (funds code)"},
        {"EQE",  -1, -1, "Equatorial Guinean ekwele"},
        {"ESA", 996, 2, "Spanish peseta (account A)"},
        {"ESB", 995, 2, "Spanish peseta (account B)"},
	/* 212 */
        {"ESP", 724, 2, "Spanish peseta"},
        {"FIM", 246, -1, "Finnish markka"},
        {"FRF", 250, 2, "French franc"},
        {"GHC", 288, -1, "Ghanaian cedi"},
	/* 216 */
        {"GNE",  -1, -1, "Guinean syli"},
        {"GRD", 300, -1, "Greek drachma"},
        {"GWP", 624, -1, "Guinea peso"},
        {"IEP", 372, -1, "Irish punt"},
	/* 220 */
        {"ILP",  -1, -1, "Israeli lira"},
        {"ILR",  -1, -1, "Israeli old sheqel"},
        {"ISJ",  -1, 2, "Icelandic old krona"},
        {"ITL", 380, 2, "Italian lira"},
	/* 224 */
        {"LAJ",  -1, -1, "Lao kip"},
        {"LUF", 442, 2, "Luxembourg franc (currency union with BEF)"},
        {"MAF",  -1, -1, "Mali franc"},
        {"MCF",  -1, -1, "Monegasque franc (currency union with FRF)"},
	/* 228 */
        {"MGF", 450, -1, "Malagasy franc"},
        {"MKN",  -1, -1, "Former Yugoslav Republic of Macedonia denar A/93"},
        {"MTL", 470, -1, "Maltese lira"},
        {"MVQ",  -1, -1, "Maldive rupee"},
	/* 232 */
        {"MXP",  -1, -1, "Mexican peso"},
        {"MZM", 508, -1, "Mozambican metical"},
        {"NLG", 528, 2, "Netherlands guilder/florin"},
        {"PEH",  -1, -1, "Peruvian sol"},
	/* 236 */
        {"PEI",  -1, -1, "Peruvian inti"},
        {"PLZ", 616, 2, "Polish zloty A/94"},
        {"PTE", 620, 2, "Portuguese escudo"},
        {"RON",  -1, -1, "Romanian leu A/52"},
	/* 240 */
        {"ROL", 642, -1, "Romanian leu A/05"},
        {"RUR", 810, -1, "Russian ruble A/97"},
        {"SDD", 736, -1, "Sudanese dinar"},
        {"SIT", 705, -1, "Slovenian tolar"},
	/* 244 */
        {"SML",  -1, -1, "San Marinese lira (currency union with ITL and VAL)"},
        {"SRG", 740, -1, "Suriname guilder/florin"},
        {"SUR",  -1, -1, "Soviet Union ruble"},
        {"SVC", 222, -1, "Salvadoran col\xc3\xb3n"},
	/* 248 */
        {"TJR", 762, -1, "Tajikistan ruble"},
        {"TPE", 626, -1, "Portuguese Timorese escudo"},
        {"TRL", 792, -1, "Turkish lira A/05"},
        {"UAK", 804, -1, "Ukrainian karbovanets"},
	/* 252 */
        {"UGS",  -1, -1, "Ugandan shilling A/87"},
        {"UYN",  -1, -1, "Uruguay old peso"},
        {"VAL",  -1, -1, "Vatican lira (currency union with ITL and SML)"},
        {"VEB", 862, -1, "Venezuelan bol\xc3\xadvar"},
	/* 256 */
        {"VNC",  -1, -1, "Vietnamese old dong"},
        {"XEU", 954, 2, "European Currency Unit (1 XEU = 1 EUR)"},
        {"XFO",  -1, -1, "Gold franc (special settlement currency)"},
        {"YDD", 720, -1, "South Yemeni dinar"},
	/* 260 */
        {"YUS",  -1, -1, "Serbian Dinar"},
        {"YUF",  -1, -1, "Federation dinar"},
        {"YUD",  -1, -1, "New Yugoslav dinar"},
        {"YUM", 891, -1, "Yugoslav dinar"},
	/* 264 */
        {"YUN",  -1, -1, "Convertible dinar"},
        {"YUR",  -1, -1, "Reformed dinar"},
        {"YUO",  -1, -1, "October dinar"},
        {"YUG",  -1, -1, "January dinar"},
	/* 268 */
        {"ZAL", 991, -1, "South African financial rand (funds code)"},
        {"ZRN", 180, -1, "Za\xc3\xafrean new za\xc3\xafre"},
        {"ZRZ",  -1, -1, "Za\xc3\xafrean za\xc3\xafre"},
        {"ZWC",  -1, -1, "Zimbabwe Rhodesian dollar"},
	/* 272 in total */
};

/* gperf me */
const_iso_4217_t
find_iso_4217_by_name(const char *name)
{
	const_iso_4217_t ep = iso_4217 + sizeof(iso_4217) / sizeof(*iso_4217);

	for (const_iso_4217_t p = iso_4217; p < ep; p++) {
		if (memcmp(p->sym, name, 3) == 0) {
			return p;
		}
	}
	return NULL;
}

/* iso4217.c ends here */
