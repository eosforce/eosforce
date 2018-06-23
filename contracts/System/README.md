# 投票

<!-- vim-markdown-toc GFM -->

* [Table](#table)
    * [accounts](#accounts)
    * [votes](#votes)
    * [bps](#bps)
    * [chianstatus](#chainstatus)
    * [schedules](#schedules)
* [Action](#action)
    * [transfer](#transfer)
    * [updatebp](#updatebp)
    * [vote](#vote)
    * [unfreeze](#unfreeze)
    * [claim](#claim)
    * [setemergency](#setemergency)

<!-- vim-markdown-toc -->

## Table

### accounts

attributes:

- {"name":"name",        "type":""},
- {"name":"available",   "type":"asset"}

```bash
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio accounts
{
  "rows": [{
      "name": "biosbpa",
      "available": "1000.0000 EOS",
    },{
      "name": "biosbpb",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpc",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpd",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpe",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpf",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpg",
      "available": "1.0000 EOS",
    },{
      "name": "biosbph",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpi",
      "available": "1.0000 EOS",
    },{
      "name": "biosbpj",
      "available": "1.0000 EOS",
    }
  ],
  "more": true
}
chain@ubuntu:~/workcode$
```

### votes

attributes:

- {"name":"bpname",                 "type":"account_name"},
- {"name":"staked",                 "type":"asset"},
- {"name":"voteage",                "type":"int64"},
- {"name":"voteage_update_height",  "type":"uint32"},
- {"name":"unstaking",              "type":"asset"},
- {"name":"unstake_height",         "type":"uint32"}

```bash
chain@ubuntu:~/workcode$ ./cleos  get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "10.0000 EOS",
      "voteage": 0,
      "voteage_update_height": 236,
      "unstaking": "0.0000 EOS",
      "unstake_height": 236
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### bps

attributes:

- {"name":"name",                  "type":"account_name"},
- {"name":"block_signing_key",     "type":"public_key"},
- {"name":"commission_rate",       "type":"uint32"},
- {"name":"total_staked",          "type":"int64"},
- {"name":"rewards_pool",          "type":"asset"},
- {"name":"total_voteage",         "type":"int64"},
- {"name":"voteage_update_height", "type":"uint32"},
- {"name":"url",                   "type":"string"},
- {"name":"emergency",             "type":"bool"}

```bash
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio bps -k biosbpa
{
  "rows": [{
      "name": "biosbpa",
      "block_signing_key": "EOS7xTPgP8HxZZSUvuoyKaw4dNGGQPHbpGYzLCYKh6evSqBNxHsWr",
      "commission_rate": 0,
      "total_staked": 0,
      "rewards_pool": "72.0000 EOS",
      "total_voteage": 0,
      "voteage_update_height": 0,
      "url": "http://eosforce.io",
      "emergency": 0
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### chainstatus

attributes:
- {"name":"name",               "type":"account_name"},
- {"name":"emergency",          "type":"bool"}

```bash
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio chainstatus
{
  "rows": [{
      "name": "chainstatus",
      "emergency": 0
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### schedules

attributes:
- {"name":"version",     "type":"uint64"},
- {"name":"block_height","type":"uint32"},
- {"name":"producers",   "type":"producer[]"}

```bash
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio schedules
{
  "rows": [{
      "version": 0,
      "block_height": 2,
      "producers": [{
          "bpname": "biosbpa",
          "amount": 3
        },{
          "bpname": "biosbpb",
          "amount": 3
        },{
          "bpname": "biosbpc",
          "amount": 3
        },{
          "bpname": "biosbpd",
          "amount": 3
        },{
          "bpname": "biosbpe",
          "amount": 3
        },{
          "bpname": "biosbpf",
          "amount": 3
        },{
          "bpname": "biosbpg",
          "amount": 3
        },{
          "bpname": "biosbph",
          "amount": 2
        },{
          "bpname": "biosbpi",
          "amount": 2
        },{
          "bpname": "biosbpj",
          "amount": 2
        },{
          "bpname": "biosbpk",
          "amount": 2
        },{
          "bpname": "biosbpl",
          "amount": 2
        },{
          "bpname": "biosbpm",
          "amount": 2
        },{
          "bpname": "biosbpn",
          "amount": 2
        },{
          "bpname": "biosbpo",
          "amount": 2
        },{
          "bpname": "biosbpp",
          "amount": 2
        },{
          "bpname": "biosbpq",
          "amount": 2
        },{
          "bpname": "biosbpr",
          "amount": 2
        },{
          "bpname": "biosbps",
          "amount": 2
        },{
          "bpname": "biosbpt",
          "amount": 2
        },{
          "bpname": "biosbpu",
          "amount": 3
        },{
          "bpname": "biosbpv",
          "amount": 3
        },{
          "bpname": "biosbpw",
          "amount": 3
        }
      ]
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode
```

## Action

### transfer

```cpp
void transfer(const account_name from, const account_name to, const asset quantity, const string /*memo*/);
```

Parameters：

- from：      EOS sender
- to：        EOS receiver
- quantity：  EOS quantity
- memo:       transaction memo

```bash
chain@ubuntu:~/workcode$ ./cleos push action eosio transfer '{"from":"eosforce","to":"biosbpa","quantity":"200.0000 EOS","memo":"my first transfer"}' -p eosforce
executed transaction: 6cd9a73c4504698120770d30b8778cc39e31433225836dad26df69fdcdb46831  160 bytes  4031 us
#         eosio <= eosio::transfer              {"from":"eosforce","to":"biosbpa","quantity":"200.0000 EOS","memo":"my first transfer"}
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$
```

### updatebp

```cpp
void updatebp(const account_name bpname,const public_key producer_key,const uint32_t commission_rate, const std::string & url);
```

Parameters：

- bpname：         block producer name
- producer_key：   block producer public key
- commission_rate：commission rate ，1 <= commission rate <= 10000 means 0.01%~100%
- url: block producer additional information,e.g: "http://eosforce.io"

```bash
chain@ubuntu:~/workcode$ ./cleos push action eosio updatebp '{"bpname":"biosbpa","block_signing_key":"EOS7xTPgP8HxZZSUvuoyKaw4dNGGQPHbpGYzLCYKh6evSqBNxHsWr","commission_rate":"100","url":"http://eosforce.io"}' -p biosbpa
executed transaction: 7bcd4b80942db4c388092f4eb00b655f73cb17def6af56c5f458a8502a9ea3b2  160 bytes  4156 us
#         eosio <= eosio::updatebp              {"bpname":"biosbpa","block_signing_key":"EOS7xTPgP8HxZZSUvuoyKaw4dNGGQPHbpGYzLCYKh6evSqBNxHsWr","com...
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$
```

### vote

```cpp
void vote(const account_name voter, const account_name bpname, const asset stake);
```

Parameters：

- voter：     vote account name
- bpname：    block producer name
- stake：     vote quantity which is a integer ，becauseof fee, so need 0 <= stake.amount < staked.amount + available.amount

```bash
chain@ubuntu:~/workcode$ ./cleos push action eosio vote '{"voter":"biosbpa","bpname":"biosbpa","stake":"10.0000 EOS"}' -p biosbpa
executed transaction: 4c3f306e59a988f489a9b3d17c2fed2e20a526c5fb9ea5dc12ddcd3ddeb5a647  144 bytes  4215 us
#         eosio <= eosio::vote                  {"voter":"biosbpa","bpname":"biosbpa","stake":"10.0000 EOS"}
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$ ./cleos get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "10.0000 EOS",
      "voteage": 0,
      "voteage_update_height": 236,
      "unstaking": "0.0000 EOS",
      "unstake_height": 236
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### unfreeze

```cpp
void unfreeze(const account_name voter ,const account_name bpname);
```

Parameters：

- voter：     vote account name
- bpname：      block producer name
- need *unstaking.amount > 0 and unstake_height + frozen_delay < current_block_num()*

```bash
chain@ubuntu:~/workcode$ ./cleos push action eosio vote '{"voter":"biosbpa","bpname":"biosbpa","stake":"35.0000 EOS"}' -p biosbpa
executed transaction: f14daa3acd06e5137fefa7da9a065f8555ea60c751238b636357440d52f6317c  144 bytes  4207 us
#         eosio <= eosio::vote                  {"voter":"biosbpa","bpname":"biosbpa","stake":"35.0000 EOS"}
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$ ./cleos get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "35.0000 EOS",
      "voteage": 1660,
      "voteage_update_height": 310,
      "unstaking": "15.0000 EOS",
      "unstake_height": 310
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$ ./cleos push action eosio unfreeze '{"voter":"biosbpa","bpname":"biosbpa"}' -p biosbpa
executed transaction: c937869baf762f3f905af5b7aa501d76de4ed10ad63bde9a13e11c9cda2372bd  128 bytes  4342 us
#         eosio <= eosio::unfreeze              {"voter":"biosbpa","bpname":"biosbpa"}
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$ ./cleos get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "35.0000 EOS",
      "voteage": 1660,
      "voteage_update_height": 310,
      "unstaking": "0.0000 EOS",
      "unstake_height": 310
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### claim

```cpp
void claim( const account_name voter ,const account_name bpname);
```

Parameters：

- voter：     vote account name
- bpname：    block producer name

```bash
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio accounts -k biosbpa
{
  "rows": [{
      "name": "biosbpa",
      "available": "1155.5900 EOS"
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$ ./cleos get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "35.0000 EOS",
      "voteage": 1660,
      "voteage_update_height": 310,
      "unstaking": "0.0000 EOS",
      "unstake_height": 310
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$ ./cleos push action eosio claim '{"voter":"biosbpa","bpname":"biosbpa"}' -p biosbpa
executed transaction: e2bdf4d630f66944d597167a0b50889cc4e5073f4d5e3597804f731ce1ad6bb6  128 bytes  5541 us
#         eosio <= eosio::claim                 {"voter":"biosbpa","bpname":"biosbpa"}
>> --claim--reward----152.2900 EOS
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$ ./cleos get table eosio biosbpa votes
{
  "rows": [{
      "bpname": "biosbpa",
      "staked": "35.0000 EOS",
      "voteage": 0,
      "voteage_update_height": 386,
      "unstaking": "0.0000 EOS",
      "unstake_height": 310
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$ ./cleos get table eosio eosio accounts -k biosbpa
{
  "rows": [{
      "name": "biosbpa",
      "available": "1307.9600 EOS"
    }
  ],
  "more": false
}
chain@ubuntu:~/workcode$
```

### setemergency

```cpp
void setemergency( const account_name bpname ,const bool emergency);
```

Parameters：

- bpname：    block producer name
- emergency： proposal which need more than 2/3 block producers set *emergency = true* will only stop

transfer/updatebp/vote/claim/unfreeze/newaccount actions, otherwise, nothing to change.

```bash
chain@ubuntu:~/workcode$ ./cleos   push action eosio setemergency '{"bpname":"biosbpa","emergency":true}' -p biosbpa
executed transaction: 5d4bbcc4e04e0ed1107d1a75b564f3b4d612a30e531853266c79ebf7dd7bd756  120 bytes  14806 us
#         eosio <= eosio::setemergency          {"bpname":"biosbpa","emergency":1}
warning: transaction executed locally, but may not be confirmed by the network yet
chain@ubuntu:~/workcode$
```
