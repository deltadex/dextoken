/**
 *  @file
 *  @copyright defined in eos/LICENSE.txt
 */

#include "dextoken.hpp"

namespace eosio {

void dextoken::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void dextoken::issue( account_name to, asset quantity, string memo )
{
    do_issue(to,quantity,memo,true);
}

void dextoken::issuefree( account_name to, asset quantity, string memo )
{
    do_issue(to,quantity,memo,false);
}

void dextoken::burn( account_name from, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before burn" );
    const auto& st = *existing;

    require_auth( from );
    require_recipient( from );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount >= 0, "must burn positive or zero quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply -= quantity;
    });

    sub_balance( from, quantity );
}

void dextoken::signup( account_name owner, asset quantity)
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before signup" );
    const auto& st = *existing;

    require_auth( owner );
    require_recipient( owner );

    accounts to_acnts( _self, owner );
    auto to = to_acnts.find( sym_name );
    eosio_assert( to == to_acnts.end() , "you have already signed up" );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount == 0, "quantity exceeds signup allowance" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( owner, quantity, owner );
}

void dextoken::transfer( account_name from, account_name to, asset quantity, string memo )
{
  do_transfer(from,to,quantity,memo,true);
}

void dextoken::transferfree( account_name from, account_name to, asset quantity, string memo )
{
  do_transfer(from,to,quantity,memo,false);
}

void dextoken::do_issue( account_name to, asset quantity, string memo, bool pay_ram )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount >= 0, "must issue positive quantity or zero" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
      if(pay_ram == true) {
        SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
      } else {
        SEND_INLINE_ACTION( *this, transferfree, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
      }
    }
}

void dextoken::do_transfer( account_name from, account_name to, asset quantity, string memo, bool pay_ram )
{
  eosio_assert( from != to, "cannot transfer to self" );
  require_auth( from );
  eosio_assert( is_account( to ), "to account does not exist");
  auto sym = quantity.symbol.name();
  stats statstable( _self, sym );
  const auto& st = statstable.get( sym );

  require_recipient( from );
  require_recipient( to );

  eosio_assert( quantity.is_valid(), "invalid quantity" );
  eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
  eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
  eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


  sub_balance( from, quantity );
  add_balance( to, quantity, from, pay_ram );
}

void dextoken::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );
   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

void dextoken::add_balance( account_name owner, asset value, account_name ram_payer, bool pay_ram)
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      eosio_assert(pay_ram == true, "destination account does not have balance");
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_ABI( eosio::dextoken, (create)(issue)(issuefree)(burn)(signup)(transfer)(transferfree) )
