"""The subject tester expects HEAD on a GET-only route to return 405."""
from tests.lib.assertions import assert_status


def test_head_policy(ctx):
    raw = ctx.send_raw_request('HEAD / HTTP/1.1\r\nHost: primary.test\r\n\r\n')
    assert_status(raw, 405)
    headers, body = raw.split('\r\n\r\n', 1)
    assert '\r\nAllow: GET' in headers
    assert body == '', 'HEAD error responses must not send a body'
    # Recognizing HEAD must not turn an unknown method into a route-policy error.
    raw = ctx.send_raw_request('UNKNOWN / HTTP/1.1\r\nHost: primary.test\r\n\r\n')
    assert_status(raw, 501)
    raw = ctx.send_raw_request('HEAD /old HTTP/1.1\r\nHost: primary.test\r\n\r\n')
    assert_status(raw, 301)
    assert raw.split('\r\n\r\n', 1)[1] == ''


def register_tests():
    return [('HEAD returns route 405 with Allow and no body; unknown methods stay 501', test_head_policy)]
