#include <boost/test/unit_test.hpp>
#include <enumivo/testing/tester.hpp>
#include <enumivo/chain/abi_serializer.hpp>
#include "enu.system_tester.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>

using namespace enumivo::testing;
using namespace enumivo;
using namespace enumivo::chain;
using namespace enumivo::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class ubitoken_tester : public tester {
public:

   ubitoken_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(ubitoken) } );
      produce_blocks( 2 );

      set_code( N(ubitoken), contracts::ubitoken_wasm() );
      set_abi( N(ubitoken), contracts::ubitoken_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(ubitoken) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer_max_time);
   }

   int64_t nowtime() {
     return control->head_block_time().time_since_epoch().count(); 
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(ubitoken);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data,abi_serializer_max_time );

      return base_tester::push_action( std::move(act), uint64_t(signer));
   }

   fc::variant get_account( account_name acc, account_name issuer)
   {
      vector<char> data = get_row_by_account( N(ubitoken), acc, N(accounts), issuer );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer_max_time );
   }

   fc::variant get_connection( account_name acc, account_name peer)
   {
      vector<char> data = get_row_by_account( N(ubitoken), acc, N(connections), peer );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "connection", data, abi_serializer_max_time );
   }

   fc::variant get_issuer( account_name acc )
   {
      vector<char> data = get_row_by_account( N(ubitoken), acc, N(issuers), acc );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "issuer", data, abi_serializer_max_time );
   }

   action_result launch( account_name genesis ) {

      return push_action( N(ubitoken), N(launch), mvo()
           ( "genesis", genesis )
      );
   }

   action_result apply( account_name acc,
                account_name referral ) {

      return push_action( acc, N(apply), mvo()
           ( "acc", acc)
           ( "referral", referral)
      );
   }

   action_result accept( account_name acc,
                account_name candidate ) {

      return push_action( acc, N(accept), mvo()
           ( "acc", acc)
           ( "candidate", candidate)
      );
   }

   action_result trust( account_name from,
                account_name to ) {

      return push_action( from, N(trust), mvo()
           ( "from", from)
           ( "to", to)
      );
   }

   action_result untrust( account_name from,
                account_name to ) {

      return push_action( from, N(untrust), mvo()
           ( "from", from)
           ( "to", to)
      );
   }

   action_result issue( account_name issuer ) {

      return push_action( issuer, N(issue), mvo()
           ( "issuer", issuer)
      );
   }

   action_result transfer( account_name from,
                  account_name to,
                  account_name issuer,
                  asset        quantity,
                  string       memo ) {
      return push_action( from, N(transfer), mvo()
           ( "from", from)
           ( "to", to)
           ( "issuer", issuer)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result swap( account_name from,
                  account_name to,
                  account_name from_token_issuer,
                  account_name to_token_issuer,
                  asset        quantity) {
      return push_action( from, N(swap), mvo()
           ( "from", from)
           ( "to", to)
           ( "from_token_issuer", from_token_issuer)
           ( "to_token_issuer", to_token_issuer)
           ( "quantity", quantity)
      );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(ubitoken_tests)


BOOST_FIXTURE_TEST_CASE( launch_tests, ubitoken_tester ) try {

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "genesis account does not exist" ), 
       launch( N(john) )
   );

   auto token = launch( N(alice) );
   auto issuer = get_issuer( N(alice) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "alice")
      ("referral", "alice")
      ("apply", "alice")
      ("last_issue_time", 0)
      ("supply", "0.0000 UBI")
      ("next_issue_quantity", "100.0000 UBI")
   );
   auto conn = get_connection( N(alice), N(alice) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "alice")
      ("trust_due_time", 0xffffffffffffffff)
      ("revocable", false)
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( apply_tests, ubitoken_tester ) try {

   launch( N(alice) );
   produce_blocks(1);
   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "referral account does not exist" ), 
       apply( N(bob), N(john) )
   );

   apply( N(bob), N(alice) );
   auto issuer = get_issuer( N(bob) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "bob")
      ("referral", "ubitoken")
      ("apply", "alice")
      ("last_issue_time", 0xffffffffffffffff)
      ("supply", "0.0000 UBI")
      ("next_issue_quantity", "0.0000 UBI")
   );
   produce_blocks(1);

   accept( N(alice), N(bob) );
   issuer = get_issuer( N(bob) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "bob")
      ("referral", "alice")
      ("apply", "alice")
      ("last_issue_time", 0)
      ("supply", "0.0000 UBI")
      ("next_issue_quantity", "100.0000 UBI")
   );
   produce_blocks(1);

   apply( N(carol), N(alice) );
   issuer = get_issuer( N(carol) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "carol")
      ("referral", "ubitoken")
      ("apply", "alice")
      ("last_issue_time", 0xffffffffffffffff)
      ("supply", "0.0000 UBI")
      ("next_issue_quantity", "0.0000 UBI")
   );
   produce_blocks(1);

   // carol change to apply to bob
   apply( N(carol), N(bob) );
   issuer = get_issuer( N(carol) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "carol")
      ("referral", "ubitoken")
      ("apply", "bob")
      ("last_issue_time", 0xffffffffffffffff)
      ("supply", "0.0000 UBI")
      ("next_issue_quantity", "0.0000 UBI")
   );
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "candidate is not applying for current issuer."),
       accept( N(alice), N(carol) )
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( accept_tests, ubitoken_tester ) try {
   launch( N(alice) );
   produce_blocks(1);

   apply( N(bob), N(alice) );
   produce_blocks(1);

   accept( N(alice), N(bob) );
   auto conn = get_connection( N(bob), N(alice) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "alice")
      ("trust_due_time", 0xffffffffffffffff)
      ("revocable", false)
   );
   conn = get_connection( N(alice), N(bob) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "bob")
      ("trust_due_time", 0xffffffffffffffff)
      ("revocable", false)
   );
   conn = get_connection( N(bob), N(bob) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "bob")
      ("trust_due_time", 0xffffffffffffffff)
      ("revocable", false)
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( trust_tests, ubitoken_tester ) try {
   launch( N(alice) );
   produce_blocks(1);

   apply( N(bob), N(alice) );
   produce_blocks(1);

   accept( N(alice), N(bob) );
   produce_blocks(1);

   apply( N(carol), N(alice) );
   produce_blocks(1);

   accept( N(alice), N(carol) );
   produce_blocks(1);


   trust( N(bob), N(carol) );

   auto conn = get_connection( N(bob), N(carol) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "carol")
      ("trust_due_time", 0xffffffffffffffff)
      ("revocable", true)
   );
   produce_blocks(1);

   untrust( N(bob), N(carol) );

   auto duetime = nowtime() + (int64_t)1000000 * 3600 * 24 * 30;
   conn = get_connection( N(bob), N(carol) );
   REQUIRE_MATCHING_OBJECT( conn, mvo()
      ("peer", "carol")
      ("trust_due_time", duetime)
      ("revocable", true)
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( issue_tests, ubitoken_tester ) try {
   launch( N(alice) );
   produce_blocks(1);

   issue( N(alice) );
   auto issuer = get_issuer( N(alice) );
   REQUIRE_MATCHING_OBJECT( issuer, mvo()
      ("issuer", "alice")
      ("referral", "alice")
      ("apply", "alice")
      ("last_issue_time", nowtime())
      ("supply", "100.0000 UBI")
      ("next_issue_quantity", "99.9900 UBI")
   );
   auto account = get_account( N(alice), N(alice) );
   REQUIRE_MATCHING_OBJECT( account, mvo()
      ("balance", "100.0000 UBI")
      ("issuer", "alice")
   );

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
