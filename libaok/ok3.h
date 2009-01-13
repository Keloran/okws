// -*-c++-*-
/* $Id$ */

#ifndef __LIBAOK_OK3_H__
#define __LIBAOK_OK3_H__

#include "ok.h"

/*
 * okclnt3_t: like okclnt_t and okclnt2_t, a class that corresponds to
 * an incoming HTTP client request.  okclnt3_t is difference since it
 * supports HTTP/1.1 pipelining, and therefore can accept multiple
 * requests per one connection.
 */
class okclnt3_t : public okclnt_interface_t {
public:

  //------------------------------------------------------------------------

  class cv_t {
  public:
    cv_t () : _go (false) {}
    void wait (evv_t ev);
    void poke ();
  private:
    bool _go;
    evv_t::ptr _ev;
  };

  //------------------------------------------------------------------------


  class req_t : public http_parser_cgi_t, public virtual refcount {
  public:
    req_t (ptr<ahttpcon> x, u_int rn, htpv_t prev_vers, u_int to);
    ~req_t ();

    typedef event<int, bool>::ref parse_ev_t;

    void parse (parse_ev_t, CLOSURE); 
    http_inhdr_t *hdr_p () { return http_parser_cgi_t::hdr_p (); }
    const http_inhdr_t &hdr_cr () const { return http_parser_cgi_t::hdr_cr (); }

    void set_union_cgi_mode (bool b)
    { http_parser_cgi_t::set_union_mode (b); }

    htpv_t http_vers () const { return hdr.get_vers (); }
  };

  //------------------------------------------------------------------------

  class resp_t : public virtual refcount {
  public:
    resp_t (okclnt3_t *o, ptr<req_t> q);
    ~resp_t ();

    //-----------------------------------------------------------------------

    void error (int status, str s = NULL) { reply (status, NULL, NULL, s); }
    void redirect (int status, str u) { reply (status, NULL, u, NULL); }
    void ok (int status, ptr<compressible_t> body) { reply (status, body); }

    void reply (int status, ptr<compressible_t> body, 
		str url = NULL, str es = NULL);

    void set_release_ev (evv_t::ptr ev) { _release_ev = ev; }

    //-----------------------------------------------------------------------

    ptr<cookie_t> add_cookie (const str &h = NULL, const str &p = "/");
    void set_uid (u_int64_t i) { _uid = i; _uid_set = true; }

    //-----------------------------------------------------------------------

    void set_content_type (const str &s) { _content_type = s; }
    void set_cache_control (const str &s) { _cache_control = s; }
    void set_expires (const str &s) { _expires = s; }
    void set_content_disposition (const str &s) { _cont_disp = s; }
    void disable_gzip () { _rsp_gzip = false; }
    void set_custom_log2 (const str &s) { _custom_log2 = s; }
    void set_hdr_field (const str &k, const str &v);

    //-----------------------------------------------------------------------

    void set_attributes (http_resp_attributes_t *hra);
    void fixup_cookies (ptr<http_response_t> rsp);
    void fixup_response (ptr<http_response_t> rsp);

    //-----------------------------------------------------------------------

    void mark_defunct () { _ok_clnt = NULL; }
    bool is_ready () const { return _replied; }
    void send (evb_t ev, CLOSURE);

    //-----------------------------------------------------------------------

    ptr<req_t> req () { return _req; }
    int status () const { return _status; }
    bool do_gzip () const;
    ptr<ahttpcon> con ();
    oksrvc_t *svc ();

    //-----------------------------------------------------------------------

  protected:
    okclnt3_t *_ok_clnt;
    vec<ptr<cookie_t> > _outcookies;
    ptr<http_response_t> _http_resp;
    u_int64_t _uid;
    bool _uid_set;

    str _content_type, _cache_control, _expires, _cont_disp;
    str _custom_log2;
    bool _rsp_gzip;

    bool _sent, _replied;

    int _status;
    ptr<compressible_t> _body;
    str _redir_url;
    str _error_str;

    ptr<vec<http_hdr_field_t> > _hdr_fields;

    ptr<req_t> _req;
    evv_t::ptr _release_ev;
  };

  //------------------------------------------------------------------------

  class rrpair_t : public virtual okrrp_interface_t  {
  public:
    rrpair_t (ptr<req_t> rq, ptr<resp_t> resp)
      : _req (rq), _resp (resp) {}
    
    void set_custom_log2 (const str &log) { _resp->set_custom_log2 (log); }
    void disable_gzip () { _resp->disable_gzip (); }
    void set_expires (const str &s)  { _resp->set_expires (s); }

    void set_hdr_field (const str &k, const str &v) 
    { _resp->set_hdr_field (k, v); }

    void set_cache_control (const str &s) { _resp->set_cache_control (s); }
    void set_content_type (const str &s) { _resp->set_content_type (s); }

    // access input parameters
    const http_inhdr_t &hdr_cr () const { return _req->hdr_cr (); }
    virtual http_inhdr_t *hdr_p () { return _req->hdr_p (); }

    // output paths...
    void okreply (ptr<compressible_t> c, evv_t::ptr ev);
    void redirect (const str &s, int status, evv_t::ptr ev);
    void error (int n, const str &s, evv_t::ptr ev);

  private:

    void output_T (compressible_t *c, evv_t::ptr ev, CLOSURE);
    void redirect_T (const str &s, int status, evv_t::ptr ev, CLOSURE);
    void error_T (int n, const str &s, bool dm, evv_t::ptr ev, CLOSURE);

    ptr<req_t> _req;
    ptr<resp_t> _resp;
  };

  //------------------------------------------------------------------------

  okclnt3_t (ptr<ahttpcon> xx, oksrvc_t *o, u_int to = 0);
  ~okclnt3_t ();

  //------------------------------------------------------------------------

  virtual void process (ptr<req_t> req, ptr<resp_t> resp, evv_t ev) = 0;

  // convert to a standard request that looks like HTTP/1.0
  ptr<rrpair_t> convert (ptr<req_t> req, ptr<resp_t> resp);

  //------------------------------------------------------------------------

  virtual bool ssl_only () const { return false; } 
  virtual str  ssl_redirect_str () const { return NULL; }
  bool is_ssl () const { return _demux_data && _demux_data->ssl (); }
  str ssl_cipher () const;

  //------------------------------------------------------------------------

  void set_localizer (ptr<const pub_localizer_t> l);
  ptr<pub2::ok_iface_t> pub2 ();
  ptr<pub2::ok_iface_t> pub2_local ();
  ptr<ahttpcon> con () { return _x; }

  //------------------------------------------------------------------------

  void set_union_cgi_mode (bool b) { _union_cgi_mode = b; }
  void set_demux_data (ptr<demux_data_t> d)  { _demux_data = d; }
  virtual void serve () { serve_T (); }

  //------------------------------------------------------------------------

protected:

  //------------------------------------------------------------------------

  void serve_T (CLOSURE);
  virtual void finish_serve () { delete this; }

  //-----------------------------------------------------------------------
  
  ptr<resp_t> alloc_resp (ptr<req_t> r = NULL);
  bool check_ssl ();
  void redirect (int status, const str &u);
  void error (int status);

  //-----------------------------------------------------------------------

  void poke ();
  void output_loop (int time_budget, evv_t ev, CLOSURE);
  void finish_output (evv_t ev);
  void await_poke (evv_t ev);

  //-----------------------------------------------------------------------

  ptr<ahttpcon> _x;
  u_int _timeout;

  ptr<demux_data_t> _demux_data;
  ptr<pub2::locale_specific_publisher_t> _p2_locale;
  vec<ptr<resp_t> > _resps;

  bool _union_cgi_mode;
  bool _serving;

  //-----------------------------------------------------------------------

  cv_t _output_cv;

  //-----------------------------------------------------------------------

};

//-----------------------------------------------------------------------


#endif /* __LIBAOK_OK3_H__ */
