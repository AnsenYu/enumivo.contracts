/**
 *  @file
 *  @copyright
 */
#pragma once

#include <enulib/asset.hpp>
#include <enulib/enu.hpp>

#include <string>

namespace enumivosystem {
   class system_contract;
}

namespace enumivo {

   using std::string;

   class [[enumivo::contract("ubitokentest")]] token : public contract {
      public:
         using contract::contract;

         [[enumivo::action]]
         void launch( name   genesis );

         [[enumivo::action]]
         void apply( name   acc, name referral );

         [[enumivo::action]]
         void accept( name  acc, name candidate );

         [[enumivo::action]]
         void trust( name  from, name to );

         [[enumivo::action]]
         void untrust( name  from, name to );

         [[enumivo::action]]
         void issue(  name  issuer );

         [[enumivo::action]]
         void transfer( name    from,
                        name    to,
                        name    issuer,
                        asset   quantity,
                        string  memo );

         [[enumivo::action]]
         void trusttransfer( name    from,
                        name    to,
                        name    issuer,
                        asset   quantity,
                        string  memo );

         [[enumivo::action]]
         void swap( name    from,
                    name    to,
                    name    from_token_issuer,
                    name    to_token_issuer,
                    asset   quantity);


         static asset get_supply( name token_contract_account, name issuer)
         {
            issuers issuerstable( token_contract_account, issuer.value );
            const auto& st = issuerstable.get( issuer.value );
            return st.supply;
         }

         static asset get_balance( name token_contract_account, name owner, name issuer)
         {
            accounts accountstable( token_contract_account, owner.value );
            const auto& ac = accountstable.get( issuer.value );
            return ac.balance;
         }
         
         static uint64_t get_last_issue_time( name token_contract_account, name issuer ) {
           issuers issuerstable( token_contract_account, issuer.value );
            const auto& is = issuerstable.get( issuer.value );
            return is.last_issue_time;
         }

         static name get_referral( name token_contract_account, name issuer ) {
           issuers issuerstable( token_contract_account, issuer.value );
            const auto& is = issuerstable.get( issuer.value );
            return is.referral;
         }

         static name get_apply( name token_contract_account, name issuer ) {
           issuers issuerstable( token_contract_account, issuer.value );
            const auto& is = issuerstable.get( issuer.value );
            return is.apply;
         }

         static void applied_assert( name token_contract_account, name issuer ) {
            issuers issuerstable( token_contract_account, issuer.value );
            auto existing = issuerstable.find( issuer.value );
            enumivo_assert( existing != issuerstable.end(), "issuer does not apply for ubi yet" );
         }

         static void accepted_assert( name token_contract_account, name issuer ) {
            applied_assert(token_contract_account, issuer);
            auto referral = get_referral(token_contract_account, issuer);
            enumivo_assert( referral != token_contract_account, "issuer is not accepted by any referral yet" );
         }

         static void not_accepted_assert( name token_contract_account, name issuer ) {
            applied_assert(token_contract_account, issuer);
            auto referral = get_referral(token_contract_account, issuer);
            enumivo_assert( referral == token_contract_account, "issuer is not accepted by any referral yet" );
         }

         static void issue_qualify_assert( name token_contract_account, name issuer ) {
            accepted_assert( token_contract_account, issuer );
            auto last_issue_time = get_last_issue_time(token_contract_account, issuer);
            enumivo_assert( current_time() > last_issue_time + issue_wait_time, "issue too early" );
         }

         static void connections_not_exceed_assert( name token_contract_account, name issuer ) {
           connections connectiontable( token_contract_account, issuer.value );
           uint32_t num = 0;
           for (const auto &conn : connectiontable) {
              num += conn.trust_due_time > current_time() ? 1 : 0;
           }
           enumivo_assert( num <= connection_maximum, "issuer's connections reach max limit.");
         }

         static void connection_assert( name token_contract_account, name from, name to) {
           connections connectiontable( token_contract_account, from.value );
           auto existing = connectiontable.find(to.value);
           enumivo_assert( existing != connectiontable.end(), "connection not exist" );
           const auto& st = *existing;
           enumivo_assert( st.trust_due_time > current_time(), "connection expire" );
         }

         struct [[enumivo::table]] account {
            asset    balance;
            name     issuer;

            uint64_t primary_key()const { return issuer.value; }
         };

         struct [[enumivo::table]] connection {
            name     peer;
            uint64_t trust_due_time;
            uint8_t  revocable;

            uint64_t primary_key()const { return peer.value; }
         };

         struct [[enumivo::table]] issuer {
            name     issuer;
            name     referral;  // default: contract _self
            name     apply;

            uint64_t last_issue_time;
            asset    supply;
            asset    next_issue_quantity;

            uint64_t primary_key()const { return issuer.value; }
         };

         static const uint64_t time_maximum = 0xffffffffffffffff;
         static const int64_t  initial_issue_quantity = 1000000; // 100.0000 UBI
         static const int64_t delta = 100; // each issue decrease 0.01 UBI
         static const int64_t referral_rewarding_percentage = 1; // 1% of each issued UBI goes to the referral
         static const uint64_t issue_wait_time  = (1000000 * 3600 * 24); // wait 1 day to issue
         static const uint64_t disconnect_wait_time = (1000000 * 3600 * 24 * 30); // wait 30 days to due
         static const uint32_t connection_maximum = 50; 

         static constexpr symbol ubi_symbol = symbol(symbol_code("UBI"), 4);

         typedef enumivo::multi_index< "accounts"_n, account > accounts;
         typedef enumivo::multi_index< "connections"_n, connection> connections;
         typedef enumivo::multi_index< "issuers"_n, issuer> issuers;

         void sub_balance( name owner, name issuer, asset value );
         void add_balance( name owner, name issuer, asset value, name ram_payer );

         void connect( name ram_payer, name  from, name to, bool revocable );
         void disconnect( name  from, name to);
         
         void internal_transfer( name    from,
                                name    to,
                                name    token_issuer,
                                asset   quantity,
                                name    ram_payer);

   };

} /// namespace enumivo
