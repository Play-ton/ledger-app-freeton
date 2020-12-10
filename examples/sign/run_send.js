const { TONClient } = require('ton-client-node-js');
const config = require('./send_config.json');
const fs = require('fs');

(async () => {
    try {
        const tonClient = await TONClient.create({
            servers: [config.network],
            log_verbose: false,
        });
        const { contracts } = tonClient;

        const signBytes = process.argv[2];
        if (!signBytes)
        {
            console.log("Usage: node sign_msg.js %signBytes%");
            process.exit(1);
        } 

        const signBytesBase64 = Buffer.from(signBytes, 'hex').toString('base64');

        const data = await fs.readFileSync('./unsigned_msg');
        const unsignedMessage = await JSON.parse(data);
        // console.log(unsignedMessage);
        // Create signed message with provided sign
        const signed = await contracts.createSignedRunMessage(
            {
                signBytesBase64: signBytesBase64,
                unsignedMessage: unsignedMessage
            });

        const result = await contracts.processRunMessage(signed);
        console.log(`Transaction ` + (result.transaction.aborted ? `aborted` : `complete`));
        await fs.unlinkSync('./unsigned_msg');
        process.exit(0);
    }
    catch (error) {
        console.error(error);
        process.exit(0);
    }

})();
