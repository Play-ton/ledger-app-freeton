const { TONClient } = require('ton-client-node-js');
const SetcodeMultisigWalletContract = require('./SetcodeMultisigWalletContract');
const config = require('./deploy_config.json');
const fs = require('fs');

(async () => {
    try {
        const tonClient = await TONClient.create({
            servers: [config.network],
            log_verbose: false,
        });
        const { contracts, crypto } = tonClient;

        masterKeys = { public: config.public_key, secret: "" };
        const deployParams = {
            package: {abi: SetcodeMultisigWalletContract.abi, imageBase64: SetcodeMultisigWalletContract.imageBase64},
            constructorParams: { owners: ["0x" + masterKeys.public], reqConfirms: 1 },
            keyPair: { public: masterKeys.public, private: "" },
            constructorHeader: {
                expire: Math.floor(Date.now() / 1000) + 600, // 10 minutes
            },
        };

        const unsignedMessage = await contracts.createUnsignedDeployMessage(deployParams);

        await fs.writeFileSync('./unsigned_deploy_msg', JSON.stringify(unsignedMessage));
        const bytesToSignHex = Buffer.from(unsignedMessage.signParams.bytesToSignBase64, 'base64').toString('hex');
        console.log(`Bytes to sign (hex): ${bytesToSignHex}`);

        // const signBytesBase64 = await crypto.naclSignDetached(
        //     { base64: unsignedMessage.signParams.bytesToSignBase64 },
        //     `c55ef68c61542d8aed4d3f21cfaa97969441865552c4d30f5be08f94268862e2${masterKeys.public}`,
        //     'Base64'
        // );
        // const signedBytes = Buffer.from(signBytesBase64, 'base64').toString('hex');
        // console.log(`Signed: ${signedBytes}`);

        process.exit(0);
    }
    catch (error) {
        console.error(error);
        process.exit(0);
    }

})();
