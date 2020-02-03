import json
import time
import os

max_producers = 23

pri_key = '5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3'
pub_key = 'EOS6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV'

bp_pri_key = '5KYQvUwt6vMpLJxqg4jSQNkuRfktDHtYDp8LPoBpYo8emvS1GfG'
bp_pub_key = 'EOS5muUziYrETi5b6G2Ev91dCBrEm3qir7PK4S2qSFqfqcmouyzCr'

sign_pri_key = '5JfjatHRwbmY8SfptFRxHnYUctfnuaxANTGDYUtkfrrBDgkh3hB'
sign_pub_key = 'EOS7R82SaGaJubv23GwXHyKT4qDCVXi66qkQrnjwmBUvdA4dyzEPG'

boot_time = time.strftime("%Y-%m-%dT%H:00:00.000", time.localtime())

genesis_data = {
   'initial_timestamp': boot_time,
   'initial_key': 'EOS1111111111111111111111111111111114T1Anm',
   'initial_configuration': {
      'max_block_net_usage': 1048576,
      'target_block_net_usage_pct': 1000,
      'max_transaction_net_usage': 524288,
      'base_per_transaction_net_usage': 12,
      'net_usage_leeway': 500,
      'context_free_discount_net_usage_num': 20,
      'context_free_discount_net_usage_den': 100,
      'max_block_cpu_usage': 1000000,
      'target_block_cpu_usage_pct': 1000,
      'max_transaction_cpu_usage': 750000,
      'min_transaction_cpu_usage': 100,
      'max_transaction_lifetime': 3600,
      'deferred_trx_expiration_window': 600,
      'max_transaction_delay': 3888000,
      'max_inline_action_size': 4096,
      'max_inline_action_depth': 4,
      'max_authority_depth': 6
   },
   'initial_account_list':[],
   'initial_producer_list':[]
}

keys = {
   'keymap' : []
}

sign_keys = {
   'keymap' : []
}

def add_account(acc, asset, pubkey, prikey):
   genesis_data['initial_account_list'].append({
      'key':pub_key,
      'asset':'%d.0000 EOS' % asset,
      'name':acc,
   })
   keys['keymap'].append([acc, prikey])

def add_bp_info(acc, pubkey, prikey):
   genesis_data['initial_producer_list'].append({
      'name':acc,
      'bpkey':pubkey,
      'commission_rate': 100,
      'url': 'https://www.eosforce.io/'
   })
   sign_keys['keymap'].append([acc, prikey])


# init accounts
def init_account():
   for t in range(ord('a'), ord('a')+max_producers):
      add_account('biosbp' + chr(t), 10000, bp_pub_key, bp_pri_key)

   add_account('eosforce', 1000000, pub_key, pri_key)
   add_account('devfund', 1, pub_key, pri_key)
   add_account('eosfund1', 1, pub_key, pri_key)
   add_account('b1', 1, pub_key, pri_key)
   add_account('eosio.msig', 10000, pub_key, pri_key)

   add_account('force.test', 100, pub_key, pri_key)
   add_account('force.ram', 100, pub_key, pri_key)
   add_account('force.cpu', 100, pub_key, pri_key)
   add_account('force.net', 100, pub_key, pri_key)
   add_account('force.config', 10000, pub_key, pri_key)

   add_account('r.token.in', 100000, pub_key, pri_key)
   add_account('r.token.out', 100000, pub_key, pri_key)
   add_account('r.acc.map', 100000, pub_key, pri_key)
   add_account('r.t.exchange', 100000, pub_key, pri_key)

   add_account('test', 1000000, pub_key, pri_key)

   for t in range(ord('a'), ord('z')+1):
      add_account('test' + chr(t), 500000, pub_key, pri_key)

   # init producer info
   for t in range(ord('a'), ord('a')+max_producers):
      add_bp_info('biosbp' + chr(t), sign_pub_key, sign_pri_key)

def make_genesis_data():
   init_account()
   with open(os.path.abspath('./genesis.json'), 'wt') as f:
      f.write(json.dumps(genesis_data, indent=4))

   with open(os.path.abspath('./key.json'), 'wt') as f:
      f.write(json.dumps(keys, indent=4))

   with open(os.path.abspath('./sigkey.json'), 'wt') as f:
      f.write(json.dumps(sign_keys, indent=4))

#print(json_str)

if __name__=="__main__":
   make_genesis_data()