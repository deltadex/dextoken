### To signup for the airdrop:
`cleos push action dexexchanges signup '{"owner":"owneraccount","quantity":"0.0000 DEX"}' -p owneraccount@active`

The signup function allows an account to create a balance entry using their own personal ram.


### To burn tokens run the command:
`cleos push action dexexchanges burn '{"from":"owneraccount","quantity":"1.0000 DEX","memo":"Lets remove DEX supply!"}' -p owneraccount@active`

The burn function burns the token from the "from account" and also reduces the supply.

The burn function makes sure you can't burn more tokens than supply.

The burn function has been modified to allow you the user to burn your zero balance if you don't want to wait for the airdrop.
