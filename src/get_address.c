#include "apdu_constants.h"
#include "globals.h"
#include "utils.h"
#include "contract.h"
#include "slice_data.h"
#include "byte_stream.h"

static uint8_t set_result_get_address() {
    uint8_t tx = 0;
    G_io_apdu_buffer[tx++] = ADDRESS_LENGTH;
    os_memmove(G_io_apdu_buffer + tx, data_context.addr_context.address, ADDRESS_LENGTH);
    tx += ADDRESS_LENGTH;
    return tx;
}

UX_STEP_NOCB(
    ux_display_address_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Verify",
      "address",
    });
UX_STEP_NOCB(
    ux_display_address_flow_2_step, 
    bnnn_paging, 
    {
      .title = "Address",
      .text = data_context.addr_context.address_str,
    });
UX_STEP_CB(
    ux_display_address_flow_3_step, 
    pb, 
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });
UX_STEP_CB(
    ux_display_address_flow_4_step, 
    pb, 
    send_response(set_result_get_address(), true),
    {
      &C_icon_validate_14,
      "Approve",
    });

UX_FLOW(ux_display_address_flow,
    &ux_display_address_flow_1_step,
    &ux_display_address_flow_2_step,
    &ux_display_address_flow_3_step,
    &ux_display_address_flow_4_step
);

void handleGetAddress(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    ContractContext_t* cc = &contract_context;
    if (p1 == P1_FIRST) {
        VALIDATE(p2 == 0 && dataLength == sizeof(cc->tvc_length), ERR_INVALID_REQUEST);
        cc->offset = 0;
        cc->tvc_length = U2BE(dataBuffer, 0);
        VALIDATE(cc->tvc_length && cc->tvc_length <= TVC_BUFFER_SIZE, ERR_INVALID_DATA);
        
        if (app_state != APP_STATE_IDLE) {
            reset_app_context();
        }

        app_state = APP_STATE_GETTING_ADDRESS;
        THROW(SUCCESS);
    } else if (p1 == P1_LAST) {
        VALIDATE(app_state == APP_STATE_GETTING_ADDRESS, ERR_INVALID_APP_STATE);
        VALIDATE(dataLength == sizeof(uint32_t), ERR_INVALID_REQUEST);
        VALIDATE(cc->tvc_length && cc->offset == cc->tvc_length && cc->tvc_length <= TVC_BUFFER_SIZE, ERR_INVALID_DATA);
        app_state = APP_STATE_IDLE;

        ByteStream_t src;
        ByteStream_init(&src, (uint8_t*)N_tvcStorage, cc->tvc_length);
        deserialize_cells_tree(&src);

        const uint32_t account_number = readUint32BE(dataBuffer);
        get_address(account_number, data_context.addr_context.address);

        if (p2 == P2_NON_CONFIRM) {
            *tx = set_result_get_address();
            THROW(SUCCESS);
        } 
        if (p2 == P2_CONFIRM) {
            AddressContext_t* context = &data_context.addr_context;
            snprintf(context->address_str, sizeof(context->address_str), "%.*h", sizeof(context->address), context->address);
            ux_flow_init(0, ux_display_address_flow, NULL);
            *flags |= IO_ASYNCH_REPLY;
            return;
        }

        THROW(ERR_INVALID_REQUEST);
    } else if (p1 == P1_MORE) {
        VALIDATE(p2 == 0, ERR_INVALID_REQUEST);
        VALIDATE(app_state == APP_STATE_GETTING_ADDRESS, ERR_INVALID_APP_STATE);

        nvm_write((char*)N_tvcStorage + cc->offset, dataBuffer, dataLength);
        cc->offset += dataLength;
        THROW(SUCCESS);
    }

    THROW(ERR_INVALID_REQUEST);
}
