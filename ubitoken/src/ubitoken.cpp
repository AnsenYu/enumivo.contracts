/**
 *  @file
 *  @copyright
 */

#include <ubitoken/ubitoken.hpp>

namespace enumivo {

void token::launch( name genesis )
{
  require_auth( _self );
  enumivo_assert( is_account( genesis ), "genesis account does not exist");
  issuers issuerstable( _self, genesis.value );
  issuerstable.emplace( _self, [&]( auto& s ) {
     s.issuer        = genesis;
     s.referral      = genesis;
     s.apply         = genesis;
     s.last_issue_time = 0;
     s.supply        = asset{ 0, ubi_symbol };
     s.next_issue_quantity = asset{ initial_issue_quantity, ubi_symbol };
  });
}

void token::apply( name issuer, name referral )
{
  require_auth( issuer );
  enumivo_assert( is_account( referral ), "referral  account does not exist");
  accepted_assert( _self, referral );
  issuers issuerstable( _self, issuer.value );
  auto existing = issuerstable.find( issuer.value );
  if ( existing == issuerstable.end() ) {
    issuerstable.emplace( issuer, [&]( auto& s ) {
       s.issuer        = issuer;
       s.referral      = _self;
       s.apply         = referral;
       s.last_issue_time = time_maximum;
       s.supply        = asset{ 0, ubi_symbol };
       s.next_issue_quantity = asset{ initial_issue_quantity, ubi_symbol };
    });
  } else {
    not_accepted_assert( _self, issuer );
    const auto& st = *existing;
    // update apply
    issuerstable.modify( st, same_payer, [&]( auto& s ) {
       s.apply         = referral;
    });
  }
}

void token::accept( name issuer, name candidate )
{
  require_auth( issuer );
  enumivo_assert( is_account( candidate ), "referral  account does not exist");

  accepted_assert(_self, issuer);
  not_accepted_assert(_self, candidate);

  auto apply = get_apply(_self, candidate);
  enumivo_assert( apply == issuer, "candidate is not applying for current issuer.");

  // update candidate's referral
  issuers issuerstable( _self, candidate.value );
  const auto& st = issuerstable.get( candidate.value );
  issuerstable.modify( st, same_payer, [&]( auto& s ) {
     s.referral = issuer;
     s.last_issue_time = 0;
     s.next_issue_quantity = asset{ initial_issue_quantity, ubi_symbol };
  });

  const bool revocable = false;
  connect(issuer, candidate, candidate, revocable);
  connect(issuer, issuer, candidate, revocable);
  connect(issuer, candidate, issuer, revocable);
}

void token::connect( name ram_payer, name from, name to, bool revocable) {
  connections_not_exceed_assert(_self, from);
  connections connectiontable( _self, from.value );
  auto existing = connectiontable.find(to.value);
  if ( existing == connectiontable.end() ) {
    connectiontable.emplace( ram_payer, [&]( auto& s ) {
       s.peer           = to;
       s.trust_due_time = time_maximum;
       s.revocable      = revocable;
    });
  } else {
    const auto& st = *existing;
    enumivo_assert( st.revocable == false, "connection is irrevocable, no need to reconnect." );
    // update trust due to maximum
    connectiontable.modify( st, same_payer, [&]( auto& s ) {
       s.trust_due_time = time_maximum;
    });
  }

}

void token::disconnect(name from, name to) {
  connections connectiontable( _self, from.value );
  auto existing = connectiontable.find(to.value);
  enumivo_assert( existing != connectiontable.end(), "connection not exists." );
  const auto& st = *existing;
  enumivo_assert( st.revocable == true, "connection is not revocable." );
  // update trust due
  connectiontable.modify( st, same_payer, [&]( auto& s ) {
     s.trust_due_time = current_time() + disconnect_wait_time; 
  });
}

void token::trust(name from, name to) {
  require_auth( from );
  enumivo_assert( is_account( to ), "to account does not exist");

  accepted_assert(_self, from);
  accepted_assert(_self, to);

  connect(from, from, to, false);
}

void token::untrust(name from, name to) {
  require_auth( from );
  enumivo_assert( is_account( to ), "to account does not exist");

  accepted_assert(_self, from);
  accepted_assert(_self, to);

  disconnect(from, to);
}

void token::issue( name issuer )
{
    require_auth( issuer );
    issue_qualify_assert( _self, issuer );

    issuers issuerstable( _self, issuer.value );
    const auto& st = issuerstable.get( issuer.value );
    auto quantity = st.next_issue_quantity;
    auto referral = st.referral;

    enumivo_assert( quantity.amount > 0, "Issuer has no more ubi to issue. Thank you for your support!!" );

    issuerstable.modify( st, same_payer, [&]( auto& s ) {
       s.last_issue_time = current_time();
       s.supply += quantity;
       s.next_issue_quantity -= asset(delta, ubi_symbol);
    });

    auto referral_quantity =  quantity * referral_rewarding_percentage / 100;
    auto issuer_quantity =  quantity - referral_quantity ;

    add_balance( issuer, issuer, issuer_quantity, issuer );
    add_balance( referral, issuer, referral_quantity, issuer );
}

void token::transfer( name    from,
                      name    to,
                      name    token_issuer,
                      asset   quantity,
                      string  memo )
{
    require_auth( from );
    enumivo_assert( memo.size() <= 256, "memo has more than 256 bytes" );
    require_recipient( from );
    require_recipient( to );
    internal_transfer( from, to, token_issuer, quantity, from);
}

void token::trusttransfer( name    from,
                      name    to,
                      name    token_issuer,
                      asset   quantity,
                      string  memo )
{
    require_auth( from );
    enumivo_assert( memo.size() <= 256, "memo has more than 256 bytes" );
    connection_assert( _self, to, token_issuer );
    require_recipient( from );
    require_recipient( to );
    internal_transfer( from, to, token_issuer, quantity, from);
}

void token::internal_transfer( name    from,
                      name    to,
                      name    token_issuer,
                      asset   quantity,
                      name    ram_payer)
{
    enumivo_assert( from != to, "cannot transfer to self" );
    enumivo_assert( is_account( to ), "to account does not exist");

    enumivo_assert( quantity.is_valid(), "invalid quantity" );
    enumivo_assert( quantity.amount > 0, "must transfer positive quantity" );
    enumivo_assert( quantity.symbol == ubi_symbol, "UBI symbol mismatch" );

    accepted_assert(_self, from);
    accepted_assert(_self, to);

    sub_balance( from, token_issuer, quantity );
    add_balance( to, token_issuer, quantity, ram_payer );
}

void token::swap( name    from,
          name    to,
          name    from_token_issuer,
          name    to_token_issuer,
          asset   quantity) {

  require_auth( from );
  connection_assert( _self, to, from_token_issuer );
  internal_transfer( from, to, from_token_issuer, quantity, from);
  internal_transfer( to, from, to_token_issuer, quantity, from);

}


void token::sub_balance( name owner, name issuer, asset value ) {
   accounts from_acnts( _self, owner.value );

   const auto& from = from_acnts.get( issuer.value, "no balance object found" );
   enumivo_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   from_acnts.modify( from, owner, [&]( auto& a ) {
         a.balance -= value;
      });
}

void token::add_balance( name owner, name issuer, asset value, name ram_payer )
{
   accounts to_acnts( _self, owner.value );
   auto to = to_acnts.find( issuer.value );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
        a.issuer  = issuer;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace enumivo

ENUMIVO_DISPATCH( enumivo::token, (apply)(accept)(trust)(untrust)(issue)(transfer)(swap) )
