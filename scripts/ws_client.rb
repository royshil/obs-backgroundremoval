# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

require 'base64'
require 'digest'
require 'json'
require 'ostruct'
require 'socket'

class WSClient
  def connect(host, port)
    @seqnum = 0

    @socket = TCPSocket.new(host, port)
    key = Base64.strict_encode64(Random.bytes(16))

    handshake = [
      'GET / HTTP/1.1',
      "Host: #{host}:#{port}",
      'Upgrade: websocket',
      'Connection: Upgrade',
      "Sec-WebSocket-Key: #{key}",
      'Sec-WebSocket-Version: 13',
      '', ''
    ].join("\r\n")

    @socket.write(handshake)

    while (line = @socket.gets); break if line == "\r\n"; end
  end

  def send_packet(payload)
    mask_key = Random.bytes(4).unpack('C*')
    bytes = payload.to_json.b

    frame = case
    when bytes.length < 126; [0x81, bytes.length | 0x80]
    when bytes.length <= 0xFFFF; [0x81, 0xFE, [bytes.length].pack('n').unpack('C*')]
    else; [0x81, 0xFF, [bytes.length].pack('Q>').unpack('C*')]
    end
    frame += mask_key + bytes.unpack('C*').each_with_index.map { |b, i| b ^ mask_key[i % 4] }

    @socket.write(frame.flatten.pack('C*'))
  end

  def receive_packet
    h = @socket.read(2)
    return nil if h.nil?

    opcode, masked = h.bytes[0] & 0x0F, (h.bytes[1] & 0x80) != 0
    payload_len = case h.bytes[1] & 0x7F
    when 126; @socket.read(2).unpack1('n')
    when 127; @socket.read(8).unpack1('Q>')
    else; h.bytes[1] & 0x7F
    end
    raise "Connection closed by OBS (Opcode 8)" if opcode == 8

    @socket.read(4) if masked

    @socket.read(payload_len)
  end

  def send_request(type, id, data = {})
    payload = {
      'op' => 6,
      'd' => {
        'requestType' => type,
        'requestId' => id.to_s,
        'requestData' => data
      }
    }

    send_packet payload
  end

  def receive_response(type, id)
    payload = JSON.parse(receive_packet, object_class: OpenStruct)

    if payload['op'] == 7 and payload['d']['requestId'] == id.to_s and payload['d']['requestType'] == type.to_s and payload['d']['requestStatus']['code'] == 100
      payload['d']['responseData']
    else
      p payload
      raise
    end
  end

  def call(type, data = {})
    @seqnum += 1
    send_request type, @seqnum, data
    receive_response type, @seqnum
  end

  def close
    @socket.close if @socket
  end

  def authenticate(password)
    hello = JSON.parse(receive_packet)
    if hello && hello['op'] == 0
      salt = hello['d']['authentication']['salt']
      challenge = hello['d']['authentication']['challenge']

      secret = Base64.strict_encode64(Digest::SHA256.digest(password + salt))
      auth_response = Base64.strict_encode64(Digest::SHA256.digest(secret + challenge))

      send_packet({
        "op" => 1,
        "d" => { "rpcVersion" => 1, "authentication" => auth_response, "eventSubscriptions" => 0 }
      })
    end

    identified = JSON.parse(receive_packet)
    raise unless identified && identified['op'] == 2
  end

  def self.open(host, port, password)
    instance = self.new
    instance.connect host, port
    instance.authenticate password
    begin
      yield instance
    ensure
      instance.close
    end
  end
end
