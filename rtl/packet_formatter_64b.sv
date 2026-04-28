// AXI4-Stream formatter (64-bit)
// Emits fixed 32B records: [timestamp | 24B payload]
// Intended to feed AXI DMA S2MM into DDR.
//
// Assumptions:
// - input packet has at least 24 payload bytes
// - extra input beats are discarded until tlast
// - direct ready propagation is used; add skid/register slice if timing requires it

module axis_packet_formatter_64b (
    input  logic        aclk,
    input  logic        aresetn,

    input  logic [63:0] s_axis_tdata,
    input  logic [7:0]  s_axis_tkeep,
    input  logic        s_axis_tvalid,
    output logic        s_axis_tready,
    input  logic        s_axis_tlast,

    output logic [63:0] m_axis_tdata,
    output logic [7:0]  m_axis_tkeep,
    output logic        m_axis_tvalid,
    input  logic        m_axis_tready,
    output logic        m_axis_tlast
);

    typedef enum logic [2:0] {
        ST_IDLE,
        ST_SEND_TS,
        ST_SEND_FIRST_PAYLOAD,
        ST_STREAM_PAYLOAD,
        ST_FLUSH
    } state_t;

    state_t state;

    logic [63:0] timestamp_counter;
    logic [63:0] latched_timestamp;

    logic [63:0] first_payload_word;
    logic [7:0]  first_payload_keep;
    logic        first_payload_last;

    // payload beat counter: 1..3
    logic [1:0] payload_beat;

    always_ff @(posedge aclk) begin
        if (!aresetn)
            timestamp_counter <= 64'd0;
        else
            timestamp_counter <= timestamp_counter + 64'd1;
    end

    always_comb begin
        s_axis_tready = 1'b0;

        m_axis_tvalid = 1'b0;
        m_axis_tdata  = 64'd0;
        m_axis_tkeep  = 8'hff;
        m_axis_tlast  = 1'b0;

        case (state)

            ST_IDLE: begin
                s_axis_tready = 1'b1;
            end

            ST_SEND_TS: begin
                m_axis_tvalid = 1'b1;
                m_axis_tdata  = latched_timestamp;
            end

            ST_SEND_FIRST_PAYLOAD: begin
                m_axis_tvalid = 1'b1;
                m_axis_tdata  = first_payload_word;
                m_axis_tkeep  = first_payload_keep;
            end

            ST_STREAM_PAYLOAD: begin
                s_axis_tready = m_axis_tready;

                m_axis_tvalid = s_axis_tvalid;
                m_axis_tdata  = s_axis_tdata;
                m_axis_tkeep  = s_axis_tkeep;
                m_axis_tlast  = s_axis_tvalid && (payload_beat == 2'd3);
            end

            ST_FLUSH: begin
                s_axis_tready = 1'b1;
            end

            default: begin
                s_axis_tready = 1'b0;
            end

        endcase
    end

    always_ff @(posedge aclk) begin
        if (!aresetn) begin
            state              <= ST_IDLE;
            latched_timestamp  <= 64'd0;
            first_payload_word <= 64'd0;
            first_payload_keep <= 8'd0;
            first_payload_last <= 1'b0;
            payload_beat       <= 2'd0;
        end else begin
            case (state)

                ST_IDLE: begin
                    if (s_axis_tvalid && s_axis_tready) begin
                        latched_timestamp  <= timestamp_counter;
                        first_payload_word <= s_axis_tdata;
                        first_payload_keep <= s_axis_tkeep;
                        first_payload_last <= s_axis_tlast;
                        state              <= ST_SEND_TS;
                    end
                end

                ST_SEND_TS: begin
                    if (m_axis_tvalid && m_axis_tready) begin
                        payload_beat <= 2'd1;
                        state        <= ST_SEND_FIRST_PAYLOAD;
                    end
                end

                ST_SEND_FIRST_PAYLOAD: begin
                    if (m_axis_tvalid && m_axis_tready) begin
                        if (first_payload_last) begin
                            state <= ST_IDLE;
                        end else begin
                            payload_beat <= 2'd2;
                            state        <= ST_STREAM_PAYLOAD;
                        end
                    end
                end

                ST_STREAM_PAYLOAD: begin
                    if (s_axis_tvalid && s_axis_tready) begin
                        if (payload_beat == 2'd3) begin
                            state <= s_axis_tlast ? ST_IDLE : ST_FLUSH;
                            payload_beat <= 2'd0;
                        end else begin
                            payload_beat <= payload_beat + 2'd1;
                        end
                    end
                end

                ST_FLUSH: begin
                    if (s_axis_tvalid && s_axis_tready && s_axis_tlast) begin
                        state        <= ST_IDLE;
                        payload_beat <= 2'd0;
                    end
                end

                default: begin
                    state <= ST_IDLE;
                end

            endcase
        end
    end

endmodule
