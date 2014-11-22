#include "bigint.hpp"
#include <iostream>
#include <climits>
using namespace std;

std::vector<std::pair<uint8_t, std::pair<const char*, std::vector<uint64_t>>>> uint64_repr_test = {
  {10, {"18", {18}}},
  {10, {"19", {19}}},
  {10, {"10", {10}}},
  {10, {"11", {11}}},
  {10, {"20", {20}}},
  {10, {"220", {220}}},
  {10, {"1220", {1220}}},
  {10, {"4220", {4220}}},
  {10, {"9220", {9220}}},
  {10, {"2939220", {2939220}}},
  {10, {"7779220", {7779220}}},
  {10, {"7659220", {7659220}}},
  {10, {"9999999", {9999999}}},
  {10, {"19999999", {19999999}}},
  {10, {"119999999", {119999999}}},
  {10, {"1119999999", {1119999999}}},
  {10, {"11119999999", {11119999999}}},
  {10, {"111119999999", {111119999999}}},
  {10, {"9111119999999", {9111119999999}}},
  {10, {"99111119999999", {99111119999999}}},
  {10, {"999111119999999", {999111119999999}}},
  {10, {"9999111119999999", {9999111119999999}}},
  {10, {"18446744073709551615", {18446744073709551615ULL}}}, // 2^64-1
  {10, {"18446744073709551616", {0ULL, 1ULL}}}, // 2^64 -- this should leave the lowest data element completely zero.
  {10, {"18446744073709551617", {1ULL, 1ULL}}}, // 2^64+1
  // Octal tests
  {8, {"10", {8}}},
  {8, {"100", {64}}},
  // TODO more
  // hex tests
  {16, {"ff", {0xff}}},
  {16, {"FF", {0xff}}},
  {16, {"fff1", {0xfff1}}},
  {16, {"1fffffffffffffff", {0x1fffffffffffffffULL}}},
  {16, {"ffffffffffffffff", {0xffffffffffffffffULL}}},
  {16, {"1ffffffffffffffff", {0xffffffffffffffffULL, 0x1}}},
  {16, {"10000000000000000", {0x0000000000000000ULL, 0x1}}},
};

void test_encoding() {
  uint64_t goodcount = 0;
  uint64_t badcount = 0;
  for (auto it = uint64_repr_test.begin(); it != uint64_repr_test.end(); ++it) {
    BigInt bi(it->second.first, it->first);

    vector<uint64_t> data = bi.get_internal_representation();
    if (data != it->second.second) {
      cout << "Fail encoding " << it->second.first << " (base " << (int)it->first << "):" << endl;
      for (auto i = data.begin(); i != data.end(); ++i) {
        cout << "  " << *i << endl;
      }
      badcount++;
    } else {
      goodcount++;
    }
  }
  cout << "string read test: " << (goodcount+badcount) << " total, " << badcount << " failed." << endl;
}

std::vector<const char *> long_inputs_dec = {
  "67324676952986348926577676611981983467567468288549287349572934587236492348756872346587324675243587",
  "122312222115236575373999887765544111773737772884999"
};

std::vector<const char *> short_inputs_hex = {
  "112233445566778899aabbccddeeff00112233445566778899aabbccddeeff00"
};
std::vector<const char *> long_inputs_hex = {
  "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
  "fabcadac8723bc7848bcbdbab2738918827737790000000000000000302137876677bbcbabdbbbaaaaccffcfcbbbbbaad78399991999000"
};

std::vector<std::vector<std::string>> long_inputs_add = {
  {"64327169238471629502469567364910832756873465735476879721010293898587498723940047598475834758738768762",
    "97789823471987728818821987959848599834752987349587877453416324786667364216900987234787547934754777236661551128937987573888372349872394793287498888737465167777",
    "97789823471987728818821987959848599834752987349587877453480651955905835846403456802152458767511650702397028008658997867786959848596334840885974723496203936539"},
  {"12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
   "98765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210",
   "111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111101111111110111111111011111111100"},
};
void test_shifts_inner(BigInt &bi, const int shift_width, uint64_t &goodcount, uint64_t &badcount) {
  auto data1 = bi.get_internal_representation();
  bi >>= shift_width;
  auto data2 = bi.get_internal_representation();
  // verify that for each block in the shifted block, the upper n bytes are equivalent to the lower n bytes in the next block in data1.
  // exception is for shifts that are multiples of 64: here we need to compare blocks directly, as we can't shl/shr on multiples of the bit size.
  uint64_t block_jump = shift_width/64;
  if (shift_width % 64)
    block_jump++; 

  for (size_t i = 0; i < data1.size()-block_jump && i < data2.size(); ++i) {
    if (shift_width % 64 != 0) {
      uint64_t mask = (1ULL << (shift_width%64))-1;
      uint8_t block_shift = 64-(shift_width%64);

      if (((data2[i] & (mask << block_shift)) >> block_shift) == (data1[i+block_jump] & mask)) {
        goodcount++;
      } else {
        cout << "FAIL: shifting @" << shift_width << ", block " << i << " to " << i + block_jump <<  endl;
        cout << hex << setw(16) << data2[i] << " <--> " << data1[i+block_jump] << dec << endl;
        badcount++;
      }
    } else {
      if (data2[i] == data1[i+block_jump]) { // -1 because we added 1 before, which is not needed in this case.
        goodcount++;
      } else { 
        cout << "FAIL: shifting @" << shift_width << " (n*64), block " << i << " to " << i + block_jump <<  endl;
        cout << hex << setw(16) << data2[i] << " <--> " << data1[i+block_jump] << dec << endl;
        badcount++;
      }
    }
  }
};

void test_shifts() {
  uint64_t goodcount = 0;
  uint64_t badcount = 0;
  
  for (auto it = long_inputs_hex.begin(); it != long_inputs_hex.end(); ++it) {
    for (size_t shift_width = 0; shift_width <= string(*it).size()*4; ++shift_width) {
      BigInt bi(*it, 16);
      test_shifts_inner(bi, shift_width, goodcount, badcount); 
    }
  }

  for (auto it = short_inputs_hex.begin(); it != short_inputs_hex.end(); ++it) {
    for (size_t shift_width = 0; shift_width <= string(*it).size()*4; ++shift_width) {
      BigInt bi(*it, 16);
      test_shifts_inner(bi, shift_width, goodcount, badcount); 
    }
  }

  for (auto it = long_inputs_dec.begin(); it != long_inputs_dec.end(); ++it) {
    for (size_t shift_width = 0; shift_width <= string(*it).size()*4; ++shift_width) {
      BigInt bi(*it, 16);
      test_shifts_inner(bi, shift_width, goodcount, badcount); 
    }
  }
  cout << "shift tests: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
}

void test_adds() {
  uint64_t goodcount = 0;
  uint64_t badcount = 0;

  uint64_t idx = 0;
  for (idx = 0; idx < long_inputs_add.size(); ++idx) {
    BigInt a(long_inputs_add[idx][0]);
    //a.dump_registers("a  ");
    BigInt a_c(a);
    //a_c.dump_registers("a_c");
    BigInt b(long_inputs_add[idx][1]);
    //b.dump_registers("b  ");
    BigInt result(long_inputs_add[idx][2]);
    //result.dump_registers("res");
    if (a != a_c) {
      cout << "FAIL: equivalence @testidx" << idx << endl;
      badcount++;
    } else {
      goodcount++;
    }
    a += b;
    if (a == result) {
      goodcount++;
    } else {
      badcount++;
      cout << "FAIL: a != result @testidx " << idx << endl;
    }
  }

  cout << "add tests: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
}

void test_add_bits_at_pos() {
  uint64_t goodcount = 0, badcount = 0;
  BigInt bi("0");
  for (uint64_t i = 0; i < 260; ++i) {
    bi.add_bits_at_pos(i % 130, 1ull);
    if (i >= 32 && i < 130) {
      uint64_t res = bi.get_bits_at_pos(i-32, 64);
      if (res != 0x00000001ffffffff) {
        cout << "ERROR: set/get bits: get_bits(" << i-32 << ", 64) == " << hex << res << dec << endl;
        badcount++;
      } else {
        goodcount++;
      }
    } else if (i == 130) {
      if (bi.get_bits_at_pos(0, 64) != 0ULL
          || bi.get_bits_at_pos(35, 64) != 0ULL
          || bi.get_bits_at_pos(67, 64) != (/* 0b1000000...*/ 0x8000ULL << 48)) {
        badcount++;
        cout << "ERROR: getbit (i==130) failed" << endl;
      } else {
        goodcount++;
      }
    } else if (i > 130) {
      if (bi.get_bits_at_pos(0, 2) != 0x2ULL) {
        badcount++;
        cout << "ERROR: i >= 130, getbit (0,2) != 2" << endl;
      } else {
        goodcount++;
      }
    }
  }
  cout << "add/get bit tests: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
}

std::vector<std::pair<uint8_t, std::vector<std::string>>> long_inputs_mul = {
  {16, {"ffffffff", "ffffffff", "fffffffe00000001"}},
  {16, {"ffffffffffffffff", "10", "ffffffffffffffff0"}},
  {16, {"ffffffffffffffff", "ffffffffffffffff", "fffffffffffffffe0000000000000001"}},
  {10, {"64327169238471629502469567364910832756873465735476879721010293898587498723940047598475834758738768762",
    "97789823471987728818821987959848599834752987349587877453416324786667364216900987234787547934754777236661551128937987573888372349872394793287498888737465167777",
    "629054252428281994977829225250454807300460884868936239424485875727138"
      "192855096460509380771457935807502405278093610881009512920122950835318"
      "226936546982795037075464402610337453596400511527831429803122300717630"
      "5897215767721311968649908347977397551077982736582074"}},
  {10, {"12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
   "98765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210987654321098765432109876543210",
   "121932631137021795226185032733866788594511507391563633592367611644557"
     "885992987901082152001356500521260478584238530711635101356484634964180"
     "575979271268846212448009449776913427830902591068411383935373248437737"
     "913595488470234720314910989178279850632506860234718573540618646105776"
     "543485749122236092059012360920580111263525898643499378616064616736777"
     "9295611949397448712086533622923332237463801111263526900"}},
};


bool test_mul() {
  uint64_t goodcount = 0, badcount = 0;
  for (size_t i = 0; i < long_inputs_mul.size(); ++i) {
    BigInt a(long_inputs_mul[i].second[0], long_inputs_mul[i].first);
    BigInt b(long_inputs_mul[i].second[1], long_inputs_mul[i].first);
    BigInt expected_result(long_inputs_mul[i].second[2], long_inputs_mul[i].first);
    BigInt result = a * b;
    if (result != expected_result) {
      badcount++;
      cout << "test_mul error at index " << i << endl;
      result.dump_registers("res");
      expected_result.dump_registers("exp");
    } else {
      goodcount++;
    }
  }
  cout << "multiplication test: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
  return false; 
}

std::vector<std::pair<uint8_t, std::vector<std::string>>> inputs_sub = {
  {16, {"ffff", "fff0", "f"}},
  {16, {"1ffffffffffffffff", "ffffffffffffffff", "10000000000000000"}},
  {16, {"10000000000000000", "1", "ffffffffffffffff"}},
  {10, {"78187954879054785487954787925078470789324327875654652617179237196169446564792923010092391012039487123905873428796234876123412364745684792656162167548548792387932478943789787436767436",
        "278476675434676233272646467585859322376671723784657864598345998765678987654321234567890034746324716236723732741874684686474575667891238746129387465578787899347288",
        "781879548790547854876763112496437945560516814080687932948025654723847"
          "887001945770113267120243851658893379833940499101598866886320038101079"
          "69687591880657553641803091478210999537420148"}},
  {10, {"141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117067982148086513282306647093844609550582231725359408128481117450284102701938521105559644622948954930381964428810975665933446128475648233786783165271201909145648566923460348610454326648213393607260249141273724587006606315588174881520920962829254091715364367892590360011330530548820466521384146951941511609433057270365759591953092186117381932611793105118548074462379962749567351885752724891227938183011949129833673362440656643086021394946395224737190702179860943702770539217176293176752384674818467669405132000568127145263560827",
        "353787593751957781857780532171226806613001927876611195909216420198938095257201065485863278865936153381827968230301952035301852968995773622599413891249721775283479131515574857242454150695950829533116861727855889075098381754637464939319255060400927701671139009848824012858361603563707660104710181942955596198946767837449448255379774726847104047534646208046684259069491293313677028989152104752162056966024058038150193511253382430035587640247496473263914199272604269922796",
        "141592653589793238462643383279502884197169399375105820974944592307816"
        "406286208998628034825342117067982148086513282306647093844609550582231"
        "725359408128480763662690350744156663325027473396142341928454087817615"
        "066449513247190380391032721297301992335972992266738955230046658419024"
        "795244397833637649727382474865231322836456659306663678508678558140885"
        "831251030862504122255432167065829056444827696881110681731386131355910"
        "767940233824513818224951688394936605118866181015981729902437497345116"
        "501091078964414483625626678181587151792707717917406072632438540122894"
        "919644732389023665039794322349087178220172931868086368854540993638031"}},
};

bool test_sub() {  
  uint64_t goodcount = 0, badcount = 0;
  for (size_t i = 0; i < inputs_sub.size(); ++i) {
    BigInt a(inputs_sub[i].second[0], inputs_sub[i].first);
    BigInt b(inputs_sub[i].second[1], inputs_sub[i].first);
    BigInt result(inputs_sub[i].second[2], inputs_sub[i].first);

    BigInt tmp(a);
    tmp -= b;

    if (tmp != result) {
      badcount++;
      cout << "test_sub error at #" << i << endl;
      result.dump_registers("exp");
      tmp.dump_registers("act");
    } else {
      goodcount++;
    }

  }

  cout << "substraction test: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
  return false;
}

std::vector<std::pair<uint8_t, std::vector<std::string>>> inputs_div = {
  {16, {"ffff",             "2", "7fff"}},
  {16, {"fffff",            "2", "7ffff"}},
  {16, {"ffffff",           "2", "7fffff"}},
  {16, {"fffffff",          "2", "7ffffff"}},
  {16, {"ffffffff",         "2", "7fffffff"}},
  {16, {"fffffffff",        "2", "7ffffffff"}},
  {16, {"ffffffffff",       "2", "7fffffffff"}},
  {16, {"fffffffffff",      "2", "7ffffffffff"}},
  {16, {"ffffffffffff",     "2", "7fffffffffff"}},
  {16, {"fffffffffffff",    "2", "7ffffffffffff"}},
  {16, {"ffffffffffffff",   "2", "7fffffffffffff"}},
  {16, {"fffffffffffffff",  "2", "7ffffffffffffff"}},
  {16, {"ffffffffffffffff", "2", "7fffffffffffffff"}},
  {16, {"fffffffffffffffff", "2", "7ffffffffffffffff"}},
  {16, {"ffffffff",         "3",  "55555555"}},
  {16, {"ffffffffffffffffffffffff", "3", "555555555555555555555555"}},
  {16, {"ffffffffffffffffffffffffffff", "235252387897234987897874329", "73"}},
  {16, {"2ec44d0e71c5f162f424c754569a688fcbe4d9416e0c3f021f2180e8ff3c7e49333ae0cfd54be6f7436915cc1238f1ac94a87be094971982b38bbd41142eaccc3e365ed0cdfb11d8d8f35ccdd5f0dd06e3b19926d60e27d6ca905a65cf77bc170951960945f316bf9327a0e2294a8126418253718c4e391ed0af071d7338825f3",
        "3cc918f9dbdb436192873d1e4d01d4b7a254794e76811c81f102b05dad15a81810ddd97cf6c62903db7fe00dafbf9a6907d9f5b4210c38689d94f3032350547ffa1fc47c48951a5308e8d3b4454d02cc50280d65fdee741d19ce2d1860d54d6a0b1ab755",
        "c4f5aeaf07e8bca57c342d91f3c404d8174be6ab495593db5108ea2dc"}},
};

bool test_div() {  
  uint64_t goodcount = 0, badcount = 0;
  for (size_t i = 0; i < inputs_div.size(); ++i) {
    BigInt a(inputs_div[i].second[0], inputs_div[i].first);
    BigInt b(inputs_div[i].second[1], inputs_div[i].first);
    BigInt result(inputs_div[i].second[2], inputs_div[i].first);

    BigInt tmp = a/b;

    
    if (tmp != result) {
      badcount++;
      cout << "test_div error at #" << i << endl;
      result.dump_registers("exp");
      tmp.dump_registers("act");
    } else {
      goodcount++;
    }
  }
  cout << "div test: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
  return false;
}

std::vector<std::pair<uint8_t, std::vector<std::string>>> inputs_strep = {
  // TODO: Once the performance problems with toString are at least halfway fixed, add more.
  {16, {
        "acb05d8db02af6e1eae37075c2dd2b37fd80cf44ccc0dc0895ef4af165d282c1c0cf5a8aba5dbe0e"
          "713f6911a3a99aa936ae957dd093a91562e6834bb78874e80a91e36b4a48a977fc6335ca98bfb74e"
          "805dfc4b2e69cba867d253e3f8c71626d6514ec559f2784bff13598d9089997e343d8dda720d886a"
          "de1350319248db321f985f91283236f6e47273ce6d216911e5ac2ae7785d3cd73e659c4db5b5c821"
          "724fecbd68377df517dfef6acab302c0862f503d11e91c768ab6486a1d85566e00af4c4b24a78b44"
          "0181617d11f6cde80b269071ea2245f45a6596eb5d8ae1a9737e21d488eaef7e85978a2fd47700f1"
          "df50b2913ab1d4ea9d3c7f6816895ac1",
        "b3350f7b30a592c55a475edd73375401a812008a905541ff06f8609a97cc65ff90ba6a8601de5100"
          "bfdce5a552d6e0950bdd5fbf40c923f40c1a970fa12643ab2659d069392f4d1412c6bd447606fd1d"
          "4edc394d94657c7eb1b0362483b1458d2f94ec2da8589f5c9a095c89d29462e872f8e6e39d2e014a"
          "75d1078af15c6b3586eb733f3b7c802d2d2510794c74bce98de6fc8767efbe3684980db670ab7a96"
          "e8cbe03b6536254569b9b91362722e999fbf462d91c4e923c30a6fd534738290019caf011b3f4f64"
          "9a351872517442b30fb0175c414e76a9eb33ccc0d8906bd73c4539fa932ffe2a89f3a1eec6ec752a"
          "4cf9c63163cf9cb5c1a1179fab4abd86",
        "65166515d171aa0d3028ddfbe2ad6bdab0f851fd32ac7526c368faf907f698106cce34599a23d8c6"
          "6c1d47fe94f58d8ec4ca9cf2a5b20ffc37ad4e8750bc79f09b139275c09aa0fc480d18368d365dd2"
          "992c36ba76a550d30935ccf16e4161c092028b0d586448418c202ab10d7a4fd45fe979e6f74af8f1"
          "8d0bed8e8b38033a17e6c5accda096169cb74c51053e598f3b4ae625f3b25624c8a854aeff2aceb6"
          "d4009e9a1a19e9ba737d0d9d19e95b42bab2d836c3fc3af8d71a1d4b30ae076dc067726771b108fe"
          "0079b4f9ed2bd9160563f324f62a90cc61e77fed62ca661d4960adefc176147a832579a0f79d1280"
          "95052669efc58cc9977e8115f632b4c5",}},
  {10, {
        "37565629951978179190059236410953116631001275027185126993085884630037806271848018"
          "59343740729896907090462424980930476411081733761715399585919144344819216971341652"
          "39160761355324490150190223422143810801312795552928341962715821497344666437817752"
          "91645668997904662064763610501435195130352621161315837969629572334530448130008013"
          "66969198372547489292103055902515084998151705361978567536257571337991207725519143"
          "98874890548386530405238443161703662166482189862762753505776791175603768102379650"
          "88844300809308495319424307896958716699639996271958883417572372163090471341265602"
          "49348288037337054659833494777947375610985456750079732804",
        "30679170681932006964165424698475399671231663824138854038632704864592318211691174"
          "96823866125893148202431098045994490260985298473751561451918181690704625343131516"
          "86043538675675692237044329626803166600712068163554113124275246526887454327679576"
          "66485053026146778894706005294220724873056097179372087056750576806752387475701072"
          "24244246962765311936602753650637629137130245835362833205232250425217858037505415"
          "78154829711199523266082460175268022549447513920767396331730524893736461579825820"
          "21675996764464480768754610024478994403530942052025131842733785829917722307859971"
          "561260675116308203364873121945238732145964182170240873724",
        "31953127854081778815734670198340202239204790530493068477373472141846638034993509"
          "19441109564846542147235224750849418856462714364162689132660105247221299543591267"
          "03723679378071812538480373283035307497336574963614574250780626803410149673726674"
          "45632299417596196737533776672340888814247010367889196711784980821546888006092157"
          "95252218639753818052146124711864961142185816087298476593415965332955339884836818"
          "72864436101998496931891684912304821923556498921971249409662460249261651751010841"
          "45482855074221185075085456679049473789792994166237740916551487643958427284349959"
          "223618806700980570923998274536936855905131526750530530249"}},
  {36, {
          "1m527midp4ye9kn3mshp20rmwdhkukb1tf8gi0u8km2y2f772fc0fc68oi6yxtk6yzuk0rh5gmtcwdo8"
          "cvc7nzh05aoogp2mf1qtvdro1eznezcfvokek352dpedz38ou8oc4db00f4ca61r20tvewdzdjnqs5hb"
          "qy4cjaqqds63hgo8awhswwkbmmyak22zezbxw145hv5q1wo6os7m381ojjbxhfkpuu3qk2980kza423z"
          "jp8tejbfpsimhfphn9v2d2bffuzdiwk79ude1zyxnxmqi48w9k7kujld5x1lielm2ddgqt9y6ylhipmd"
          "izm73pfzmjcfbnaj2gp0l0o3chjurekyh5ot7ez16lf95afhf2xx51bwpbvb9jj08spt22vtxx3ju",
        "icrcvswhmbrka717e70kp1hfl8aux0plmn39d6z6uo0deg65rz5oh7oqk52nsqbow72jhyd9jhlr9p7l"
          "ddsa7z4o2v4fdzcxm159tvoglliyqd86dbg2rde04sdkrk9ts6nadppp3axuklp4lkoqyu356fwlwot9"
          "ixnjd0gqlnp0xozkhdj63mrfsxxkukhbet04va7wmrkkt2mkepw3ssg37oxnz5gu6m72118b2v7yzktk"
          "qty38bveg0jhd6hul4noulljpbr03th5s0zmtye379136hux4tc1t522uzbxya4xv36snwznlzh76znz"
          "l40j1klsr8nj9mtog2dqz973c68nafokpzz109uhfedtgxllsjb7zf1heyo1came4citg5779vdt"}}
};
void test_strrep() {
  // using add-tests for now, because there are some really large numbers in there ;)
  // TODO: test hex, oct, any other representation.
  uint64_t goodcount = 0, badcount = 0;
  for (auto it = inputs_strep.begin(); it != inputs_strep.end(); ++it) {
    for (auto it_inner = it->second.begin(); it_inner != it->second.end(); ++it_inner) {
      BigInt bi(*it_inner, it->first);
      string result = bi.toString(it->first, false);
      if (result != *it_inner) {
        cout << "error: from/to string conversion." << endl << "expected: " << *it_inner << endl
          << "actual  : " << result << endl;
        badcount++;
      } else {
        goodcount++;
      }
    }
  }
  cout << "toString test: " << (goodcount + badcount) << " total, " << badcount << " failed." << endl;
  return;
}

int main() {
  test_encoding();
  test_shifts();
  test_adds();
  test_add_bits_at_pos();
  test_mul();
  test_sub();
  test_div();
  test_strrep();
}

