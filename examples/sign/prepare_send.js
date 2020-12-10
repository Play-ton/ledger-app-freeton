const { TONClient } = require('ton-client-node-js');
const SetcodeMultisigWalletContract = require('./SetcodeMultisigWalletContract');
const config = require('./prepare_msg_config.json');
const fs = require('fs');

(async () => {
    try {
        const tonClient = await TONClient.create({
            servers: [config.network],
            log_verbose: false,
        });
        const { contracts } = tonClient;

        masterKeys = { public: config.public_key, secret: "" };
        const TONContractRunParams = {
            address: config.src_address,
            abi: SetcodeMultisigWalletContract.abi,
            functionName: "sendTransaction",
            header: {
                expire: Math.floor(Date.now() / 1000) + 600, // 10 minutes
            },
            input: { "dest": config.dst_address, "value": config.value, "bounce": false, "flags": 0, "payload": "" },
            keyPair: masterKeys
        };

        const unsignedMessage = await contracts.createUnsignedRunMessage(TONContractRunParams);
        await fs.writeFileSync('./unsigned_msg', JSON.stringify(unsignedMessage));
        const bytesToSignHex = Buffer.from(unsignedMessage.signParams.bytesToSignBase64, 'base64').toString('hex');
        console.log(`Bytes to sign (hex): ${bytesToSignHex}`);

        process.exit(0);
    }
    catch (error) {
        console.error(error);
        process.exit(0);
    }

})();
