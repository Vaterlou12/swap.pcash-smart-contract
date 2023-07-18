#include "swap.pcash.hpp"

swap::swap(name receiver, name code, datastream<const char *> ds)
    : contract::contract(receiver, code, ds)
{
}

void swap::open(const name &owner, const symbol &symbol, const name &ram_payer)
{
    require_auth(ram_payer);
    check(is_account(owner), "open : owner account does not exist");

    auto sym_code_raw = symbol.code().raw();
    stats statstable(get_self(), sym_code_raw);
    const auto &st = statstable.get(sym_code_raw, "open : symbol does not exist");
    check(st.supply.symbol == symbol, "open : symbol precision mismatch");

    accounts _accounts(get_self(), owner.value);
    auto it = _accounts.find(sym_code_raw);

    if (it == _accounts.end())
    {
        _accounts.emplace(ram_payer, [&](auto &a) {
            a.balance = asset(0, symbol);
        });

        create_inheritance(owner, ram_payer);
    }
}

void swap::close(const name &owner, const symbol &symbol)
{
    require_auth(owner);
    accounts _accounts(get_self(), owner.value);
    auto it = _accounts.find(symbol.code().raw());
    check(it != _accounts.end(), "close : Balance row already deleted or never existed. Action won't have any effect.");
    check(it->balance.amount == 0, "close : Cannot close because the balance is not zero.");
    _accounts.erase(it);

    if (_accounts.begin() == _accounts.end())
    {
        close_inheritance(owner);
    }
}

void swap::create_token(const name &issuer, const asset &maximum_supply)
{
    require_auth(get_self());
    check(is_account(issuer), "create_token : issuer account not exist");

    auto sym = maximum_supply.symbol;
    check(sym.is_valid(), "create_token : invalid symbol name");
    check(maximum_supply.is_valid(), "create_token : invalid supply");
    check(maximum_supply.amount > 0, "create_token : max-supply must be positive");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing == statstable.end(), "create_token : token with symbol already exists");

    statstable.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = maximum_supply.symbol;
        s.max_supply = maximum_supply;
        s.issuer = issuer;
    });
}

void swap::withdraw(const name &owner, const asset &lq_tokens)
{
    require_auth(owner);
    check(is_pool_exist(lq_tokens.symbol.code()), "withdraw : pool is not exist");
    auto pool_id = get_pool_id(lq_tokens.symbol.code());
    check(lq_tokens.amount > 0, "withdraw : amount should be positive");
    auto [token1, token2] = count_earnings_amounts(lq_tokens);
    check(is_account_exist(owner, token1.get_extended_symbol()), "withdraw : account is not exist");
    check(is_account_exist(owner, token2.get_extended_symbol()), "withdraw : account is not exist");
    sub_pool_balance(pool_id, token1, token2);
    extend_inheritance(owner, owner);
    send_retire(owner, lq_tokens, "swap.pcash: withdraw");
    send_transfer(token1.contract, owner, token1.quantity, "swap.pcash: withdraw");
    send_transfer(token2.contract, owner, token2.quantity, "swap.pcash: withdraw");
    send_rmv_lq_details(pool_id, owner, lq_tokens, token1, token2);
}

void swap::issue(const name &to, const asset &quantity, const std::string &memo)
{
    check(is_account(to), "issue : to account is not exist");
    auto sym = quantity.symbol;
    check(sym.is_valid(), "issue : invalid symbol name");
    check(memo.size() <= 256, "issue : memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "issue : token with symbol does not exist, create token before issue");
    const auto &st = *existing;

    require_auth(st.issuer);
    check(quantity.is_valid(), "issue : invalid quantity");
    check(quantity.amount > 0, "issue : must issue positive quantity");

    check(quantity.symbol == st.supply.symbol, "issue : symbol precision mismatch");
    check(quantity.amount <= st.max_supply.amount - st.supply.amount, "issue : quantity exceeds available supply");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply += quantity;
    });

    add_balance(to, quantity, st.issuer);
}

void swap::retire(const name &from, const asset &quantity, const std::string &memo)
{
    check(is_account(from), "retire : from account is not exist");

    auto sym = quantity.symbol;
    check(sym.is_valid(), "retire : invalid symbol name");
    check(memo.size() <= 256, "retire : memo has more than 256 bytes");

    stats statstable(get_self(), sym.code().raw());
    auto existing = statstable.find(sym.code().raw());
    check(existing != statstable.end(), "retire : token with symbol does not exist");
    const auto &st = *existing;

    require_auth(st.issuer);

    bool status = is_lq_tokens(extended_symbol(quantity.symbol, get_self()));

    if (!status)
    {
        check(from == st.issuer, "retire : can retire from issuer only");
    }
    check(quantity.is_valid(), "retire : invalid quantity");
    check(quantity.amount > 0, "retire : must retire positive quantity");

    check(quantity.symbol == st.supply.symbol, "retire : symbol precision mismatch");

    statstable.modify(st, same_payer, [&](auto &s) {
        s.supply -= quantity;

        if(!status)
        {
            s.max_supply -= quantity;
        }
    });

    sub_balance(from, quantity);
}

void swap::transfer_token(const name &from, const name &to,
                          const asset &quantity, const std::string &memo)
{
    check(from != to, "transfer_token : cannot transfer to self");
    require_auth(from);
    check(is_account(to), "transfer_token : to account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable(get_self(), sym.raw());
    const auto &st = statstable.get(sym.raw());

    require_recipient(from);
    require_recipient(to);

    check(quantity.is_valid(), "transfer_token : invalid quantity");
    check(quantity.amount > 0, "transfer_token : must transfer positive quantity");
    check(quantity.symbol == st.supply.symbol, "transfer_token : symbol precision mismatch");
    check(memo.size() <= 256, "transfer_token : memo has more than 256 bytes");

    auto payer = has_auth(to) ? to : from;

    sub_balance(from, quantity);
    add_balance(to, quantity, payer);
    extend_inheritance(from, payer);
    on_transfer_self_token(from, to, quantity, memo); 
}

void swap::create_pool(const name &creator, const extended_symbol &token1, const extended_symbol &token2)
{
    require_auth(creator);
    check(token1.get_symbol().is_valid(), "create_pool : token1 symbol is not valid");
    check(token2.get_symbol().is_valid(), "create_pool : token2 symbol is not valid");
    check(token1 != token2, "create_pool : tokens should be not equal");
    check(is_token_exist(token1), "create_pool : token1 is not exist");
    check(is_token_exist(token2), "create_pool : token2 is not exist");
    check(!is_pool_exist(token1, token2), "create_pool : pool already exist");

    pools _pools(get_self(), get_self().value);
    auto id = get_new_pool_id(_pools.available_primary_key());
    auto lq_symbol = to_pool_symbol(id);

    _pools.emplace(creator, [&](auto &a) {
        a.id = id;
        a.code = lq_symbol.code();
        a.pool_fee = pool_fee;
        a.platform_fee = platform_fee;
        a.fee_receiver = FEE_RECEIVER_ACCOUNT;
        a.create_time = current_time_point();
        a.last_update_time = current_time_point();
        a.token1 = extended_asset(0, token1);
        a.token2 = extended_asset(0, token2);
    });

    stats statstable(get_self(), lq_symbol.code().raw());
    auto it = statstable.find(lq_symbol.code().raw());
    check(it == statstable.end(), "create_pool : liquidity tokens already exist");

    statstable.emplace(get_self(), [&](auto &s) {
        s.supply.symbol = lq_symbol;
        s.max_supply = asset(asset::max_amount, lq_symbol);
        s.issuer = get_self();
    });
}

void swap::remove_pool(const uint64_t &pool_id)
{
    pools _pools(get_self(), get_self().value);
    auto it = _pools.find(pool_id);
    check(it != _pools.end(), "remove_pool : pool is not exist");
    auto supply = get_lq_supply(it->code);
    check(supply.amount == 0 && it->token1.quantity.amount == 0 && it->token2.quantity.amount == 0, "remove_pool : can not remove pool because liquidity and pool tokens supply is not zero");
    stats statstable(get_self(), it->code.raw());
    const auto &obj = statstable.get(it->code.raw(), "no stat object found");
    statstable.erase(obj);
    _pools.erase(it);
}

void swap::distribute_inheritance(const name &initiator, const name &inheritance_owner, const symbol_code &token)
{
    require_auth(initiator);
    inheritance _inheritance(get_self(), get_self().value);
    auto cur_date = current_time_point().sec_since_epoch();
    auto it = _inheritance.find(inheritance_owner.value);
    check(it != _inheritance.end(), "distribute_inheritance : inheritance_owner is not exist");
    check(it->inheritance_date.sec_since_epoch() < cur_date, "distribute_inheritance : inheritance date is not expired");

    accounts from_acnts(get_self(), it->user_name.value);
    auto iter = from_acnts.find(token.raw());
    check(iter != from_acnts.end(), "distribute_inheritance : token is not exist");
    check(iter->balance.amount > 0, "distribute_inheritance : distribute amount should be positive");

    if (it->inheritors.size() == 1 && it->inheritors.back().inheritor == FEE_RECEIVER_ACCOUNT)
    {
        add_inh_balance(it->user_name, FEE_RECEIVER_ACCOUNT, iter->balance, initiator);
    }
    else
    {
        add_inh_balances(it->user_name, iter->balance, it->inheritors, initiator);
    }
    sub_balance(it->user_name, iter->balance);
    send_notify("inheritance", it->user_name, name(), -iter->balance, "");
}

void swap::update_inheritance_date(const name &owner, const uint32_t &inactive_period)
{
    require_auth(owner);
    inheritance _inheritance(get_self(), get_self().value);
    auto it = _inheritance.find(owner.value);
    check(it != _inheritance.end(), "update_inheritance_date : account is not found");
    check(is_valid_inactive_period(inactive_period), "update_inheritance_date : invalid inactive period");
    auto date = get_inheritance_exp_date(inactive_period);

    _inheritance.modify(it, owner, [&](auto &w) {
        w.inheritance_date = date;
        w.inactive_period = inactive_period;
    });
}

void swap::update_inheritors(const name &owner, const std::vector<inheritor_record> &inheritors)
{
    require_auth(owner);
    inheritance _inheritance(get_self(), get_self().value);
    auto it = _inheritance.find(owner.value);
    check(it != _inheritance.end(), "update_inheritors : account is not found");
    check(is_not_self_in_inheritors(owner, inheritors), "update_inheritors : owner can not be in inheritors list");
    check(is_valid_inheritors_amount(inheritors.size()), "update_inheritors : invalid inheritors amount");
    check(is_inheritors_unique(inheritors), "update_inheritors : inheritors must be unique");
    check(is_valid_inheritors(inheritors), "update_inheritors : invalid inheritors shares or accounts");

    _inheritance.modify(it, owner, [&](auto &w) {
        w.inheritors = inheritors;
    });
}

void swap::swap_details(const uint64_t &pool_id, const name &owner, const extended_asset &token_in, const extended_asset &token_out, const extended_asset &pool_fee, const extended_asset &platform_fee, const double &price)
{
    require_auth(get_self());
    require_recipient(owner);
}

void swap::add_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2)
{
    require_auth(get_self());
    require_recipient(owner);
}

void swap::remove_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2)
{
    require_auth(get_self());
    require_recipient(owner);
}

void swap::notify(const std::string &action_type, const name &to, const name &from, const asset &quantity, const std::string &memo)
{
    require_auth(get_self());
    require_recipient(to);
}

void swap::on_transfer_self_token(const name &from, const name &to, const asset &quantity, const std::string &memo)
{
    if (to == get_self())
    {
        if (is_swap_memo(memo))
        {
            do_swap(from, quantity, memo, "on_transfer : ");
        }
        else if (is_deposit_memo(memo))
        {
            do_deposit(from, quantity, memo, "on_transfer : ");
        }
        else
        {
            check(false, "on_transfer : invalid transaction");
        }
    }
}

void swap::on_transfer(const name &from, const name &to, const asset &quantity, const std::string &memo)
{
    if (to == get_self())
    {
        if (is_swap_memo(memo))
        {
            do_swap(from, quantity, memo, "on_transfer : ");
        }
        else if (is_deposit_memo(memo))
        {
            do_deposit(from, quantity, memo, "on_transfer : ");
        }
        else
        {
            check(false, "on_transfer : invalid transaction");
        }
    }
}

void swap::do_swap(const name &from, const asset &quantity, const std::string &memo, const std::string &assert_prefix)
{
    auto params = to_key_value(memo);
    auto [status, pool_ids, min_amount] = is_valid_swap_memo(params);
    check(status, assert_prefix + "invalid swap memo");
    check(is_pools_exist(pool_ids), assert_prefix + "invalid pool ids in swap memo");
    check(min_amount > 0, assert_prefix + "invalid min amount in swap memo");
    extended_asset income(quantity, get_first_receiver());

    if (pool_ids.size() == 1)
    {
        check(is_pool_match(pool_ids[0], income), assert_prefix + "pool is not matched with tokens");
        check(income.quantity.amount >= min_swap_amount, assert_prefix + "invalid min swap amount");
        auto [amount_in, amount_out, pool_fee, platform_fee, fee_receiver, price] = count_swap_amounts(pool_ids[0], income);
        check(amount_out.quantity.amount >= min_amount, assert_prefix + "amount out less than min required");
        check(is_account_exist(from, amount_out.get_extended_symbol()), assert_prefix + "account for swap amount out is not exist");

        add_pool_balance(pool_ids[0], amount_in + pool_fee);
        sub_pool_balance(pool_ids[0], amount_out);

        send_swap_details(pool_ids[0], from, income, amount_out, pool_fee, platform_fee, price);
        send_transfer(platform_fee.contract, fee_receiver, platform_fee.quantity, "swap.pcash: swap fee");
        send_transfer(amount_out.contract, from, amount_out.quantity, "swap.pcash: swap token");
    }
    else
    {
        auto temp_income = income;

        for (auto i(0); i < pool_ids.size(); ++i)
        {
            check(is_pool_match(pool_ids[i], temp_income), assert_prefix + "pool is not matched with tokens");
            check(temp_income.quantity.amount >= min_swap_amount, assert_prefix + "invalid min swap amount");
            auto [amount_in, amount_out, pool_fee, platform_fee, fee_receiver, price] = count_swap_amounts(pool_ids[i], temp_income);

            add_pool_balance(pool_ids[i], amount_in + pool_fee);
            sub_pool_balance(pool_ids[i], amount_out);

            send_swap_details(pool_ids[i], from, temp_income, amount_out, pool_fee, platform_fee, price);
            send_transfer(platform_fee.contract, fee_receiver, platform_fee.quantity, "swap.pcash: swap fee");

            if (i == pool_ids.size() - 1)
            {
                check(amount_out.quantity.amount >= min_amount, assert_prefix + "amount out less than min required");
                check(is_account_exist(from, amount_out.get_extended_symbol()), assert_prefix + "account for swap amount out is not exist");
                send_transfer(amount_out.contract, from, amount_out.quantity, "swap.pcash: swap token");
            }

            temp_income = amount_out;
        }
    }
}

void swap::do_deposit(const name &from, const asset &quantity, const std::string &memo, const std::string &assert_prefix)
{
    auto params = to_key_value(memo);
    auto [status, pool_id] = is_valid_deposit_memo(params);
    check(status, assert_prefix + "invalid deposit memo");
    check(is_pool_exist(pool_id), assert_prefix + "invalid pool id in deposit memo");
    auto trx = get_income_trx();
    auto deposits = parse_deposit_actions(trx);
    check(is_valid_deposits(deposits), assert_prefix + "invalid deposits");
    check(is_pool_match(pool_id, deposits[0].quantity, deposits[1].quantity), assert_prefix + "pool is not matched with tokens");
    deposit current_deposit{from, extended_asset(quantity, get_first_receiver()), memo};

    if (is_last_deposit(current_deposit, deposits))
    {
        auto [lq_amount, token1, token2, rest] = count_add_lq_amounts(pool_id, deposits[0].quantity, deposits[1].quantity);
        check(is_account_exist(from, extended_symbol(lq_amount.symbol, get_self())), assert_prefix + "liquidity balance account is not exist");

        add_pool_balance(pool_id, token1, token2);
        extend_inheritance(from, same_payer);
        if (rest.quantity.amount > 0)
        {
            send_transfer(rest.contract, from, rest.quantity, "swap.pcash: deposit refund");
        }
        send_issue(from, lq_amount, "swap.pcash: add liquidity");
        send_add_lq_details(pool_id, from, lq_amount, token1, token2);
    }
}

void swap::add_balance(const name &owner, const asset &value, const name &ram_payer)
{
    accounts to_acnts(get_self(), owner.value);
    auto to = to_acnts.find(value.symbol.code().raw());
    if (to == to_acnts.end())
    {
        to_acnts.emplace(ram_payer, [&](auto &a) {
            a.balance = value;
        });

        create_inheritance(owner, ram_payer);
    }
    else
    {
        to_acnts.modify(to, same_payer, [&](auto &a) {
            a.balance += value;
        });
    }
}

void swap::sub_balance(const name &owner, const asset &value)
{
    accounts from_acnts(get_self(), owner.value);
    const auto &from = from_acnts.get(value.symbol.code().raw(), "no balance object found");
    check(from.balance.amount >= value.amount, "overdrawn balance");

    from_acnts.modify(from, same_payer, [&](auto &a) {
        a.balance -= value;
    });
}

void swap::add_pool_balance(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");

    _pools.modify(pool, same_payer, [&](auto &a) {
        a.token1 += token1;
        a.token2 += token2;
        a.last_update_time = current_time_point();
    });
}

void swap::sub_pool_balance(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");
    check(pool.token1 >= token1, "overdrawn token1 pool balance");
    check(pool.token2 >= token2, "overdrawn token2 pool balance");

    _pools.modify(pool, same_payer, [&](auto &a) {
        a.token1 -= token1;
        a.token2 -= token2;
        a.last_update_time = current_time_point();
    });
}

void swap::add_pool_balance(const uint64_t &pool_id, const extended_asset &tokens)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");

    _pools.modify(pool, same_payer, [&](auto &a) {
        if (tokens.get_extended_symbol() == pool.token1.get_extended_symbol())
        {
            a.token1 += tokens;
        }
        else
        {
            a.token2 += tokens;
        }
        a.last_update_time = current_time_point();
    });
}

void swap::sub_pool_balance(const uint64_t &pool_id, const extended_asset &tokens)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");

    _pools.modify(pool, same_payer, [&](auto &a) {
        if (tokens.get_extended_symbol() == pool.token1.get_extended_symbol())
        {
            check(tokens < pool.token1, "overdrawn token1 pool balance");
            a.token1 -= tokens;
        }
        else
        {
            check(tokens < pool.token2, "overdrawn token2 pool balance");
            a.token2 -= tokens;
        }
        a.last_update_time = current_time_point();
    });
}

void swap::create_inheritance(const name &owner, const name &ram_payer)
{
    inheritance _inheritance(get_self(), get_self().value);
    auto it = _inheritance.find(owner.value);
    if (it == _inheritance.end())
    {
        _inheritance.emplace(ram_payer, [&](auto &a) {
            a.user_name = owner;
            a.inheritance_date = time_point_sec(current_time_point().sec_since_epoch() + max_inh_period);
            a.inactive_period = max_inh_period;
            a.inheritors = std::vector<inheritor_record>{{FEE_RECEIVER_ACCOUNT, max_percent}};
        });
    }
}

void swap::close_inheritance(const name &owner)
{
    inheritance _inheritance(get_self(), get_self().value);
    auto inh = _inheritance.find(owner.value);
    if (inh != _inheritance.end())
    {
        _inheritance.erase(inh);
    }
}

void swap::extend_inheritance(const name &owner, const name &ram_payer)
{
    inheritance _inheritance(get_self(), get_self().value);
    auto it = _inheritance.find(owner.value);
    if (it != _inheritance.end())
    {
        time_point_sec new_inh_date(current_time_point().sec_since_epoch() + it->inactive_period);

        _inheritance.modify(it, ram_payer, [&](auto &r) {
            r.inheritance_date = new_inh_date;
        });
    }
}

void swap::add_inh_balances(const name &owner, const asset &value, const std::vector<inheritor_record> &inheritors, const name &ram_payer)
{
    send_inheritance(owner, value, inheritors, 1, ram_payer);
}

void swap::add_inh_balance(const name &from, const name &to, const asset &value, const name &ram_payer)
{
    add_balance(to, value, ram_payer);
    send_notify("inheritance", to, from, value, "");
}

void swap::send_inheritance(const name &owner, const asset &quantity,
                            const std::vector<inheritor_record> &inheritors,
                            const int64_t &min_amount, const name &ram_payer)
{
    if (quantity.amount >= min_amount)
    {
        asset sum(0, quantity.symbol);
        for (auto it = inheritors.rbegin(); it != inheritors.rend(); ++it)
        {
            auto amount = count_share(quantity, it->share);
            sum += amount;
            if (it != --inheritors.rend())
            {
                add_inh_balance(owner, it->inheritor, amount, ram_payer);
            }
            else
            {
                asset rest = quantity - sum;
                add_inh_balance(owner, it->inheritor, amount + rest, ram_payer);
            }
        }
    }
    else
    {
        add_inh_balance(owner, inheritors.begin()->inheritor, quantity, ram_payer);
    }
}

std::vector<deposit>
swap::parse_deposit_actions(const transaction &trx)
{
    std::vector<deposit> result;

    for (auto act : trx.actions)
    {
        if (act.name == name("transfer"))
        {
            auto data = act.data_as<transfer_action>();
            if (data.to == get_self() && is_deposit_memo(data.memo))
            {
                auto quantity = data.quantity;
                result.push_back({data.from, extended_asset(quantity, act.account), data.memo});
            }
        }
    }

    return result;
}

std::vector<std::string> swap::split(const std::string &s, const std::string &delimiter)
{
    size_t pos_start = 0, pos_end, delim_len = delimiter.length();
    std::string token;
    std::vector<std::string> res;

    while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
    {
        token = s.substr(pos_start, pos_end - pos_start);
        pos_start = pos_end + delim_len;
        res.push_back(token);
    }

    res.push_back(s.substr(pos_start));
    return res;
}

std::map<std::string, std::string> swap::to_key_value(const std::string &memo)
{
    std::map<std::string, std::string> m;

    std::string::size_type key_pos = 0;
    std::string::size_type key_end;
    std::string::size_type val_pos;
    std::string::size_type val_end;

    while ((key_end = memo.find(':', key_pos)) != std::string::npos)
    {
        if ((val_pos = memo.find_first_not_of(":", key_end)) == std::string::npos)
            break;

        val_end = memo.find(';', val_pos);
        m.emplace(memo.substr(key_pos, key_end - key_pos), memo.substr(val_pos, val_end - val_pos));

        key_pos = val_end;
        if (key_pos != std::string::npos)
            ++key_pos;
    }

    return m;
}

asset swap::count_share(const asset &quantity, const asset &share)
{
    double result = (double)quantity.amount * (double)share.amount;
    result /= (double)max_percent.amount;
    return asset(result, quantity.symbol);
}

asset swap::count_lq_tokens(const asset &supply, const extended_asset &amount1_in, const extended_asset &amount1_before)
{
    auto result = (double)supply.amount * (double)amount1_in.quantity.amount / (double)amount1_before.quantity.amount;
    return asset(result, supply.symbol);
}

std::tuple<asset, extended_asset, extended_asset, extended_asset>
swap::count_deposit_amounts(const asset &lq_supply, const pool &current_pool, const extended_asset &token1, const extended_asset &token2)
{
    auto pool_price = (double)current_pool.token1.quantity.amount / (double)current_pool.token2.quantity.amount;
    extended_asset amount1_in(pool_price * token2.quantity.amount, token1.get_extended_symbol());

    if (amount1_in < token1)
    {
        auto rest = token1 - amount1_in;
        auto lq_tokens = count_lq_tokens(lq_supply, amount1_in, current_pool.token1);
        if (rest.quantity.amount == 1)
            return std::make_tuple(lq_tokens, amount1_in + rest, token2, extended_asset());
        else
            return std::make_tuple(lq_tokens, amount1_in, token2, rest);
    
    }
    else if (token1 < amount1_in)
    {
        extended_asset amount2_in(token1.quantity.amount / pool_price, token2.get_extended_symbol());
        auto rest = token2 - amount2_in;
        auto lq_tokens = count_lq_tokens(lq_supply, amount2_in, current_pool.token2);
        if (rest.quantity.amount == 1)
            return std::make_tuple(lq_tokens, token1, amount2_in + rest, extended_asset());
        else
            return std::make_tuple(lq_tokens, token1, amount2_in, rest);
    }
    else
    {
        extended_asset amount2_in(token1.quantity.amount / pool_price, token2.get_extended_symbol());

        if (amount2_in < token2)
        {
            auto rest = token2 - amount2_in;
            auto lq_tokens = count_lq_tokens(lq_supply, token1, current_pool.token1);
            if (rest.quantity.amount == 1)
                return std::make_tuple(lq_tokens, token1, amount2_in + rest, extended_asset());
            else
                return std::make_tuple(lq_tokens, token1, amount2_in, rest);
        }
        else if (token2 < amount2_in)
        {
            auto lq_tokens = count_lq_tokens(lq_supply, token1, current_pool.token1);
            return std::make_tuple(lq_tokens, token1, token2, extended_asset());
        }
        else
        {
            auto lq_tokens = count_lq_tokens(lq_supply, token1, current_pool.token1);
            return std::make_tuple(lq_tokens, token1, token2, extended_asset());
        }
    }
}

std::tuple<asset, extended_asset, extended_asset, extended_asset>
swap::count_add_lq_amounts(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");
    auto supply = get_lq_supply(pool.code);

    if (is_initial_add_lq(supply, pool.token1, pool.token2))
    {
        auto value = std::sqrt(token1.quantity.amount * token2.quantity.amount);
        return std::make_tuple(asset(value, supply.symbol), token1, token2, extended_asset());
    }
    else
    {
        auto [lq_tokens, token1_in, token2_in, rest] = count_deposit_amounts(supply, pool, token1, token2);
        return std::make_tuple(lq_tokens, token1_in, token2_in, rest);
    }
}

std::tuple<extended_asset, extended_asset>
swap::count_earnings_amounts(const asset &lqtokens)
{
    auto supply = get_lq_supply(lqtokens.symbol.code());
    auto [token1, token2] = get_pool_tokens(lqtokens.symbol.code());
    auto amount1 = (double)lqtokens.amount / (double)supply.amount * token1.quantity.amount;
    auto amount2 = (double)lqtokens.amount / (double)supply.amount * token2.quantity.amount;
    return std::make_tuple(extended_asset(amount1, token1.get_extended_symbol()), extended_asset(amount2, token2.get_extended_symbol()));
}

extended_asset swap::count_platform_fee(const asset &platform_fee, const extended_asset &income)
{
    return income.quantity.amount <= 2000 ? extended_asset(1, income.get_extended_symbol())
                                          : extended_asset((double)platform_fee.amount / (double)10000 * income.quantity.amount, income.get_extended_symbol());
}

std::tuple<extended_asset, extended_asset>
swap::count_swap_fees(const extended_asset &income, const asset &pool_fee, const asset &platform_fee)
{
    auto total_fee = pool_fee + platform_fee;
    extended_asset sum_fee((double)total_fee.amount / (double)10000 * income.quantity.amount, income.get_extended_symbol());
    auto plt_fee = count_platform_fee(platform_fee, income);
    return std::make_tuple(sum_fee - plt_fee, plt_fee);
}

std::tuple<extended_asset, extended_asset, extended_asset, extended_asset, name, double>
swap::count_swap_amounts(const uint64_t &pool_id, const extended_asset &income)
{
    pools _pools(get_self(), get_self().value);
    const auto &pool = _pools.get(pool_id, "no pool object found");

    auto [pool_fee, platform_fee] = count_swap_fees(income, pool.pool_fee, pool.platform_fee);
    auto amount_in = income - pool_fee - platform_fee;
    auto k = (double)pool.token1.quantity.amount * (double)pool.token2.quantity.amount;

    if (amount_in.get_extended_symbol() == pool.token1.get_extended_symbol())
    {
        auto total_token1 = pool.token1 + amount_in;
        extended_asset total_token2(k / (double)total_token1.quantity.amount, pool.token2.get_extended_symbol());
        auto amount_out = pool.token2 - total_token2;
        auto price = (double)amount_out.quantity.amount / (double)amount_in.quantity.amount;
        return std::make_tuple(amount_in, amount_out, pool_fee, platform_fee, pool.fee_receiver, price);
    }
    else
    {
        auto total_token2 = pool.token2 + amount_in;
        extended_asset total_token1(k / (double)total_token2.quantity.amount, pool.token1.get_extended_symbol());
        auto amount_out = pool.token1 - total_token1;
        auto price = (double)amount_out.quantity.amount / (double)amount_in.quantity.amount;
        return std::make_tuple(amount_in, amount_out, pool_fee, platform_fee, pool.fee_receiver, price);
    }
}

uint64_t swap::get_new_pool_id(const uint64_t &available_id)
{
    return available_id == 0 ? 1 : available_id;
}

symbol swap::to_pool_symbol(uint64_t pool_id)
{
    std::string code = "LQ";
    std::string s;
    while (pool_id > 0)
    {
        uint32_t rem = pool_id % 26;
        if (rem == 0)
            rem = 26;
        s += ('A' + rem - 1);
        pool_id = (pool_id - rem) / 26;
    }
    std::reverse(s.begin(), s.end());
    code += s;
    return symbol(symbol_code(code.c_str()), 0);
}

std::vector<uint64_t> swap::to_uint64_ids(const std::vector<std::string> &str)
{
    std::vector<uint64_t> result;
    for (auto i : str)
    {
        result.push_back(std::stoull(i));
    }
    return result;
}

asset swap::get_lq_supply(const symbol_code &token)
{
    stats statstable(get_self(), token.raw());
    const auto &obj = statstable.get(token.raw(), "no stat object found");
    return obj.supply;
}

std::tuple<extended_asset, extended_asset> swap::get_pool_tokens(const symbol_code &pool_code)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bycode")>();
    const auto &pool = index.get(pool_code.raw(), "pool object not found");
    return std::make_tuple(pool.token1, pool.token2);
}

transaction swap::get_income_trx()
{
    auto size = transaction_size();
    char buff[size];
    auto readed_size = read_transaction(buff, size);
    check(readed_size == size, "get_income_trx : read transaction failed");
    transaction trx = unpack<transaction>(buff, size);
    return trx;
}

uint64_t swap::get_pool_id(const symbol_code &code)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bycode")>();
    const auto &pool = index.get(code.raw(), "no pool object found");
    return pool.id;
}

time_point_sec swap::get_inheritance_exp_date(const uint32_t &inactive_period)
{
    auto current_time = current_time_point().sec_since_epoch();
    time_point_sec temp(current_time + inactive_period);
    return temp;
}

bool swap::is_account_exist(const name &owner, const extended_symbol &token)
{
    accounts _accounts(token.get_contract(), owner.value);
    auto it = _accounts.find(token.get_symbol().code().raw());
    return it != _accounts.end() ? true : false;
}

bool swap::is_valid_deposits(const std::vector<deposit> &deposits)
{
    return (deposits.size() == 2 && deposits[0].from == deposits[1].from && deposits[0].memo == deposits[1].memo) ? true : false;
}

bool swap::is_initial_add_lq(const asset &supply, const extended_asset &token1, const extended_asset &token2)
{
    return (supply.amount == 0 && token1.quantity.amount == 0 && token2.quantity.amount == 0) ? true : false;
}

bool swap::is_token_exist(const extended_symbol &token)
{
    stats _stats(token.get_contract(), token.get_symbol().code().raw());
    auto it = _stats.find(token.get_symbol().code().raw());
    return it != _stats.end() && it->supply.symbol == token.get_symbol() ? true : false;
}

bool swap::is_lq_tokens(const extended_symbol &token)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bycode")>();
    auto it = index.find(token.get_symbol().code().raw());
    return it != index.end() && token.get_contract() == get_self() ? true : false;
}

bool swap::is_pool_exist(const uint64_t &pool_id)
{
    pools _pools(get_self(), get_self().value);
    auto it = _pools.find(pool_id);
    return it != _pools.end() ? true : false;
}

bool swap::is_pool_exist(const symbol_code &code)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bycode")>();
    auto it = index.find(code.raw());
    return it != index.end() ? true : false;
}

bool swap::is_pool_exist(const extended_symbol &token1, const extended_symbol &token2)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bypair")>();
    auto hash1 = to_pair_hash(token1, token2);
    auto hash2 = to_pair_hash(token2, token1);
    auto it1 = index.find(hash1);
    auto it2 = index.find(hash2);
    return (it1 != index.end() || it2 != index.end()) ? true : false;
}

bool swap::is_pools_exist(const std::vector<uint64_t> &pool_ids)
{
    for (const auto &id : pool_ids)
    {
        if (!is_pool_exist(id))
            return false;
    }
    return true;
}

bool swap::is_pool_match(const uint64_t &pool_id, const extended_asset &income)
{
    pools _pools(get_self(), get_self().value);
    const auto &obj = _pools.get(pool_id, "no pool object found");
    auto symb = income.get_extended_symbol();
    return symb == obj.token1.get_extended_symbol() || symb == obj.token2.get_extended_symbol() ? true : false;
}

bool swap::is_pool_match(const uint64_t &pool_id, const extended_asset &token1, const extended_asset &token2)
{
    pools _pools(get_self(), get_self().value);
    auto index = _pools.get_index<name("bypair")>();
    auto hash = to_pair_hash(token1.get_extended_symbol(), token2.get_extended_symbol());
    auto it = index.find(hash);
    return it != index.end() && it->id == pool_id ? true : false;
}

bool swap::is_last_deposit(const deposit &current_deposit, const std::vector<deposit> &deposits)
{
    return current_deposit == deposits[1] ? true : false;
}

bool swap::is_swap_memo(const std::string &memo)
{
    return swap_prefix == memo.substr(0, swap_prefix.size()) ? true : false;
}

bool swap::is_deposit_memo(const std::string &memo)
{
    return deposit_prefix == memo.substr(0, deposit_prefix.size()) ? true : false;
}

std::tuple<bool, std::vector<uint64_t>, uint64_t>
swap::is_valid_swap_memo(const std::map<std::string, std::string> &params)
{
    auto sw_it = params.find("swap");
    auto min_it = params.find("min");

    if (params.size() == 1 && sw_it != params.end())
    {
        auto result = split(sw_it->second, "-");
        check(is_digit(result), "is_valid_swap_memo : invalid pool ids");
        return std::make_tuple(true, to_uint64_ids(result), (uint64_t)1);
    }
    else if (params.size() == 2 && sw_it != params.end() && min_it != params.end())
    {
        check(is_digit(min_it->second), "is_valid_swap_memo : invalid min amount");
        auto result = split(sw_it->second, "-");
        check(is_digit(result), "is_valid_swap_memo : invalid pool ids");
        return std::make_tuple(true, to_uint64_ids(result), std::stoull(min_it->second));
    }
    return std::make_tuple(false, std::vector<uint64_t>(), (uint64_t)1);
}

bool swap::is_digit(const std::string &str)
{
    return str.find_first_not_of("0123456789") == std::string::npos ? true : false;
}

bool swap::is_digit(const std::vector<std::string> &str)
{
    for (auto i : str)
    {
        if (!is_digit(i))
            return false;
    }
    return true;
}

std::tuple<bool, uint64_t>
swap::is_valid_deposit_memo(const std::map<std::string, std::string> &params)
{
    auto it = params.find("deposit");

    if (params.size() == 1 && it != params.end())
    {
        check(is_digit(it->second), "is_valid_deposit_memo : invalid pool id");
        return std::make_tuple(true, std::stoull(it->second));
    }
    else
    {
        return std::make_tuple(false, (uint64_t)0);
    }
}

bool swap::is_valid_inactive_period(const uint32_t &inactive_period)
{
    return (inactive_period >= min_inh_period && inactive_period <= max_inh_period) ? true : false;
}

bool swap::is_not_self_in_inheritors(const name &owner, const std::vector<inheritor_record> &inheritors)
{
    for (const auto &i : inheritors)
    {
        if (i.inheritor == owner)
        {
            return false;
        }
    }
    return true;
}

bool swap::is_inheritors_unique(const std::vector<inheritor_record> &inheritors)
{
    std::map<name, asset> mymap;
    for (auto const &it : inheritors)
    {
        mymap.emplace(it.inheritor, it.share);
    }
    return (mymap.size() == inheritors.size() ? true : false);
}

bool swap::is_valid_inheritors_amount(const size_t &size)
{
    return ((size >= 1 && size <= 3) ? true : false);
}

bool swap::is_valid_share(const asset &share)
{
    return (share.symbol == inh_percent && share >= min_percent && share <= max_percent) ? true : false;
}

bool swap::is_valid_share_sum(const asset &sum)
{
    return (sum == max_percent ? true : false);
}

bool swap::is_valid_inheritors(const std::vector<inheritor_record> &inheritors)
{
    asset share_sum(0, inh_percent);
    for (auto &it : inheritors)
    {
        if (!is_account(it.inheritor) || !is_valid_share(it.share))
        {
            return false;
        }
        share_sum += it.share;
    }

    return (!is_valid_share_sum(share_sum) ? false : true);
}

void swap::send_issue(const name &to, const asset &quantity, const std::string &memo)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("issue"),
        std::make_tuple(to, quantity, memo))
        .send();
}

void swap::send_retire(const name &from, const asset &quantity, const std::string &memo)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("retire"),
        std::make_tuple(from, quantity, memo))
        .send();
}

void swap::send_transfer(const name &contract, const name &to, const asset &quantity, const std::string &memo)
{
    action(
        permission_level{get_self(), name("active")},
        contract,
        name("transfer"),
        std::make_tuple(get_self(), to, quantity, memo))
        .send();
}

void swap::send_swap_details(const uint64_t &pool_id, const name &owner, const extended_asset &token_in,
                             const extended_asset &token_out, const extended_asset &pool_fee,
                             const extended_asset &platform_fee, const double &price)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("swapdetails"),
        std::make_tuple(pool_id, owner, token_in, token_out, pool_fee, platform_fee, price))
        .send();
}

void swap::send_add_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("addlqdetails"),
        std::make_tuple(pool_id, owner, lqtoken, token1, token2))
        .send();
}

void swap::send_rmv_lq_details(const uint64_t &pool_id, const name &owner, const asset &lqtoken, const extended_asset &token1, const extended_asset &token2)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("rmvlqdetails"),
        std::make_tuple(pool_id, owner, lqtoken, token1, token2))
        .send();
}

void swap::send_notify(const std::string &action_type, const name &to, const name &from, const asset &quantity, const std::string &memo)
{
    action(
        permission_level{get_self(), name("active")},
        get_self(),
        name("notify"),
        std::make_tuple(action_type, to, from, quantity, memo))
        .send();
}
