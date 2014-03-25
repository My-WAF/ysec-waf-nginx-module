/*
** @file: ngx_yy_sec_waf_processor.c
** @description: This is the rule processor for yy sec waf.
** @author: dw_liqi1<liqi1@yy.com>
** @date: 2013.07.17
** Copyright (C) YY, Inc.
*/

#include "ngx_yy_sec_waf.h"


static ngx_http_yy_sec_waf_rule_t uncommon_content_type = {
    .mod = 0,
    .rule_id = 1,
};

static ngx_http_yy_sec_waf_rule_t uncommon_post_format = {
    .mod = 0,
    .rule_id = 2,
};

static ngx_http_yy_sec_waf_rule_t uncommon_post_boundary = {
    .mod = 0,
    .rule_id = 3,
};

static ngx_http_yy_sec_waf_rule_t special_file_charactor = {
    .mod = 0,
    .rule_id = 1201,
};

static ngx_http_yy_sec_waf_rule_t uncommon_hex_encoding = {
    .mod = 0,
    .rule_id = 1202,
};

static ngx_http_yy_sec_waf_rule_t uncommon_filename_postfix = {
    .mod = 0,
    .rule_id = 1203,
};

static ngx_http_yy_sec_waf_rule_t uncommon_filename = {
    .mod = 0,
    .rule_id = 1204,
};

static ngx_http_yy_sec_waf_rule_t too_many_post_args = {
    .mod = 0,
    .rule_id = 1205,
};

/* For those unused mod rules, we just set mod flag as false. */
ngx_http_yy_sec_waf_rule_t *mod_rules[] = {
    &uncommon_hex_encoding,
    &uncommon_content_type,
    &uncommon_post_format,
    &uncommon_post_boundary,
    &special_file_charactor,
    &uncommon_filename_postfix,
    &uncommon_filename,
    &too_many_post_args,
    NULL
};

const ngx_uint_t mod_rules_num = sizeof(mod_rules)/sizeof(ngx_http_yy_sec_waf_rule_t*) - 1;

static ngx_int_t ngx_http_yy_sec_waf_process_multipart(ngx_http_request_t* r,
    ngx_str_t* str, ngx_http_request_ctx_t* ctx);
static ngx_int_t ngx_http_yy_sec_waf_process_basic_rule(ngx_http_request_t *r,
    ngx_str_t *str, ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx);
static ngx_int_t ngx_http_yy_sec_waf_apply_mod_rule(ngx_http_request_t *r,
    ngx_str_t *str, ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx);

#define yy_sec_waf_apply_mod_rule(r, str, rule, ctx) do {        \
    ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,     \
        "[ysec_waf] apply mod rule in func:%s line:%d",               \
        __func__, __LINE__);                                     \
    if (rule.mod) {                                              \
        ngx_http_yy_sec_waf_apply_mod_rule(r, str, &rule, ctx);  \
        if (ctx->matched)                                        \
            return NGX_OK;                                       \
    } else                                                       \
        return NGX_ERROR;                                        \
} while (0)

/*
** @description: This function is called to apply the mod rule of yy sec waf.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @para: ngx_http_request_ctx_t *ctx
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_apply_mod_rule(ngx_http_request_t *r,
    ngx_str_t *str, ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx)
{
    if (rule == NULL || ctx == NULL)
        return NGX_ERROR;

    if (rule->mod) {
        ngx_http_yy_sec_waf_process_basic_rule(r, str, rule, ctx);

        if (ctx->matched) {
            ctx->is_wlr = rule->is_wlr;
            ctx->rule_id = rule->rule_id;
            ctx->block = rule->block;
            ctx->log = rule->log;
            ctx->gids = rule->gids;
            ctx->msg = rule->msg;
            ctx->matched_string = str;
        }
    }

    return NGX_OK;
}

/*
** @description: This function is called to process basic rule of the request.
** @para: ngx_str_t *str
** @para: ngx_http_yy_sec_waf_rule_t *rule
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_basic_rule(ngx_http_request_t *r,
    ngx_str_t *str, ngx_http_yy_sec_waf_rule_t *rule, ngx_http_request_ctx_t *ctx)
{
    int rc;

    if (str == NULL && rule->mod) {
        ctx->matched = 1;
        return NGX_OK;
    }

    if (str == NULL)
        return NGX_ERROR;

    if (rule->regex != NULL) {
        /* REGEX */
        rc = ngx_http_regex_exec(r, rule->regex, str);
        
        if (rc == NGX_OK) {
            ctx->matched = 1;
        }

        return rc;
    } else if (rule->str != NULL) {
        /* STR */
        if (ngx_strnstr(str->data, (char*) rule->str->data, str->len)) {
            ctx->matched = 1;
            return NGX_OK;
        }

        return NGX_DECLINED;
    }

    return NGX_ERROR;
}

/*
** @description: This function is called to process basic rules of the request.
** @para: ngx_http_request_t *r
** @para: ngx_str_t *str
** @para: ngx_array_t *rules
** @para: ngx_http_request_ctx_t *ctx
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_basic_rules(ngx_http_request_t *r,
    ngx_str_t *str, ngx_array_t *rules, ngx_http_request_ctx_t *ctx)
{
    int        rc;
    ngx_uint_t i;
    ngx_http_yy_sec_waf_rule_t *rule_p;

    if (rules == NULL)
        return NGX_ERROR;

    rule_p = rules->elts;

    for (i = 0; i < rules->nelts; i++) {
        rc = ngx_http_yy_sec_waf_process_basic_rule(r, str, &rule_p[i], ctx);

        if (rc == NGX_ERROR)
            return rc;

        if (ctx->matched)
            break;
    }

    if (ctx->matched) {
        ctx->is_wlr = rule_p[i].is_wlr;
        ctx->rule_id = rule_p[i].rule_id;
        ctx->block = rule_p[i].block;
        ctx->log = rule_p[i].log;
        ctx->gids = rule_p[i].gids;
        ctx->msg = rule_p[i].msg;
        ctx->matched_string = str;
    }

    return NGX_OK;
}

/*
** @description: This function is called to process spliturl rules of the request.
** @para: ngx_http_request_t *r
** @para: ngx_str_t *str
** @para: ngx_array_t *rules
** @para: ngx_http_request_ctx_t *ctx
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_spliturl_rules(ngx_http_request_t *r,
    ngx_str_t *str, ngx_array_t *rules, ngx_http_request_ctx_t *ctx)
{
    u_char    *start, *buffer, *eq, *ev;
    ngx_uint_t len, arg_cnt, arg_len, nullbytes, buffer_size;
    ngx_str_t  value;

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] data=%p", str->data);

    buffer = start = str->data;
    len =  str->len;
    buffer_size = arg_len = 0;

    if (len != 0)
        arg_cnt = 1;

    while ((start < str->data + len) && *start) {
        if (*start == '&') {
            ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] start=%p, buffer=%p", start, buffer);
            buffer_size++;
            *buffer++ = '$';
            arg_cnt++;
            start++;
            continue;
        }

        eq = (u_char*)ngx_strlchr((u_char*)start, (u_char*)str->data + len, '=');
        ev = (u_char*)ngx_strlchr((u_char*)start, (u_char*)str->data + len, '&');

        if (eq) {
            if (!ev)
                ev = str->data + str->len;
            arg_len = ev - start;
            eq = ngx_strlchr(start, start+arg_len, '=');
            if (!eq)
                return NGX_ERROR;

            eq++;
            value.data = eq;
            value.len = ev - eq;
        } else {
            break;
        }

        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] value=%V, len=%d", &value, value.len);

        nullbytes = ngx_yy_sec_waf_unescape(&value);

        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] value=%V, nullbytes=%d", &value, nullbytes);

        if (nullbytes > 0) {
            ctx->process_body_error = 1;
            ngx_str_set(&ctx->process_body_error_msg, "UNCOMMON_HEX_ENCODING");
            return NGX_ERROR;
        }

        buffer = ngx_cpymem(buffer, value.data, value.len);
        buffer_size += value.len;

        start += arg_len;
    }

    str->len = buffer_size;

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] str=%V", str);

    /* convert \r\n to blank as '  ' to improve the format of error log */
    buffer = str->data;

    while (buffer_size-- > 0) {
        if (*buffer == '\n' || *buffer == '\r')
            *buffer = ' ';
        buffer++;
    }

    if (r->method == NGX_HTTP_POST && arg_cnt > 2048)
        yy_sec_waf_apply_mod_rule(r, NULL, too_many_post_args, ctx);

    return ngx_http_yy_sec_waf_process_basic_rules(r, str, rules, ctx);
}

/*
** @description: This function is called to process the boundary of the request.
** @para: ngx_http_request_t *r
** @para: ngx_http_request_ctx_t *ctx
** @para: ngx_str_t full_body
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_boundary(ngx_http_request_t *r,
    u_char **boundary, ngx_uint_t *boundary_len)
{
    u_char *start;
    u_char *end;

    start = r->headers_in.content_type->value.data + ngx_strlen("multipart/form-data;");
    end = r->headers_in.content_type->value.data + r->headers_in.content_type->value.len;

    while (start < end && *start && (*start == ' ' || *start == '\t'))
        start++;

    if (ngx_strncmp(start, "boundary=", ngx_strlen("boundary=")))
        return NGX_ERROR;

    start += ngx_strlen("boundary=");

    *boundary_len = end - start;
    *boundary = start;

    if (*boundary_len > 70)
        return NGX_ERROR;

    return NGX_OK;
}

/*
** @description: This function is called to process the disposition of the request.
** @para: ngx_http_request_t *r
** @para: ngx_http_request_ctx_t *ctx
** @para: ngx_str_t full_body
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_disposition(ngx_http_request_t *r,
    u_char *str, u_char *line_end, ngx_str_t *name, ngx_str_t *filename)
{
    u_char *name_start, *name_end, *filename_start, *filename_end;

    while (str < line_end) {
        while(str < line_end && *str && (*str == ' ' || *str == '\t'))
            str++;
        if (str < line_end && *str && *str == ';')
            str++;
        while (str < line_end && *str && (*str == ' ' || *str == '\t'))
            str++;

        if (str >= line_end || !*str)
            break;

        if (!ngx_strncmp(str, "name=\"", ngx_strlen("name=\""))) {
            name_start = name_end = str + ngx_strlen("name=\"");
            do {
                name_end = (u_char*) ngx_strchr(name_end, '"');
                if (name_end && *(name_end - 1) != '\\')
                    break;
                name_end++;
            } while (name_end && name_end < line_end);

            if (!name_end || !*name_end)
                return NGX_ERROR;

            str = name_end;

			if (str < line_end + 1)
                str++;
            else
                return NGX_ERROR;

            name->data = name_start;
            name->len = name_end - name_start;
        } 
        else if (!ngx_strncmp(str, "filename=\"", ngx_strlen("filename=\""))) {
            filename_end = filename_start = str + ngx_strlen("filename=\"");
            do {
                /* ignore 0x00 for %00 injection situation */
                filename_end = (u_char*) ngx_strlchr(filename_end, line_end, '"');
                if (filename_end && *(filename_end - 1) != '\\')
                    break;
                filename_end++;
            } while (filename_end && filename_end < line_end);

            if (!filename_end)
                return NGX_ERROR;

            str = filename_end;
            if (str < line_end + 1)
                str++;
            else
                return NGX_ERROR;

            filename->data = filename_start;
            filename->len = filename_end - filename_start;
        }
        else if (str == line_end - 1)
            break;
		else {
            return NGX_ERROR;
		}
    }

    if (filename_end > line_end || name_end > line_end)
        return NGX_ERROR;

    return NGX_OK;
}

/*
** @description: This function is called to process the multipart of the request.
** @para: ngx_http_request_t *r
** @para: ngx_http_request_ctx_t *ctx
** @para: ngx_str_t full_body
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_multipart(ngx_http_request_t *r,
    ngx_str_t *full_body, ngx_http_request_ctx_t *ctx)
{
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_multipart Entry");

    u_char *boundary, *line_start, *line_end, *body_end, *p;
    ngx_uint_t boundary_len, idx, nullbytes;
    ngx_str_t name, filename, content_type;

    boundary = NULL;
    boundary_len = 0;

    if (r == NULL || full_body == NULL || ctx == NULL)
        return NGX_ERROR;

	if (ngx_http_yy_sec_waf_process_boundary(r, &boundary, &boundary_len) != NGX_OK) {
        yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_boundary, ctx);
	}

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] boundary: %s", boundary);

    idx = 0;

    p = ngx_strlcasestrn(full_body->data, full_body->data+full_body->len, boundary, boundary_len-1);
    if (p == NULL) {
        return NGX_ERROR;
    }

    full_body->len = full_body->len - (p - full_body->data - 2);
    full_body->data = p - 2;

    while (idx < full_body->len) {
		ngx_log_debug2(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] request_body: %s, len: %d", full_body->data+idx, full_body->len);

        if (idx+boundary_len+6 == full_body->len || idx+boundary_len+4 == full_body->len) {
            if (ngx_strncmp(full_body->data+idx, "--", 2)
                || ngx_strncmp(full_body->data+idx+2, boundary, boundary_len)
                || ngx_strncmp(full_body->data+idx+boundary_len+2, "--", 2)) {
                yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_boundary, ctx);
            } else
                break;
        }

        if ((full_body->len-idx < 4+boundary_len)
            || ngx_strncmp(full_body->data+idx, "--", 2)
            || ngx_strncmp(full_body->data+idx+2, boundary, boundary_len)
            || idx+boundary_len+2+2 >= full_body->len
            || ngx_strncmp(full_body->data+idx+boundary_len+2, "\r\n", 2)) {
            yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_boundary, ctx);
        }

        /* plus with 4 for -- and \r\n*/
        idx += boundary_len + 4;
        if (ngx_strncasecmp(full_body->data+idx, (u_char*)"content-disposition: form-data;",
            ngx_strlen("content-disposition: form-data;"))) {
            yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_format, ctx);
        }

        idx += ngx_strlen("content-disposition: form-data;");

        /* ignore 0x00 for %00 injection situation */
        line_end = (u_char*) ngx_strlchr(full_body->data+idx, full_body->data+full_body->len, '\n');
        if (!line_end) {
            return NGX_ERROR;
        }

        ngx_memzero(&name, sizeof(ngx_str_t));
        ngx_memzero(&filename, sizeof(ngx_str_t));
        ngx_memzero(&content_type, sizeof(ngx_str_t));

        ngx_http_yy_sec_waf_process_disposition(r, full_body->data+idx, line_end, &name, &filename);

        if (filename.data) {
            line_start = line_end + 1;
            line_end = (u_char*) ngx_strchr(line_start, '\n');
            if (!line_end) {
                yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_format, ctx);
            }

            content_type.data = line_start + ngx_strlen("content-type: ");
            content_type.len = (line_end - 1) - content_type.data;
        }

        idx += (u_char*)line_end - (full_body->data + idx) + 1;
        if (full_body->data[idx] != '\r' || full_body->data[idx+1] != '\n') {
            yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_format, ctx);
        }

        idx += 2;
        body_end = NULL;

        while (idx < full_body->len) {
            body_end = (u_char*) ngx_strstr(full_body->data+idx, "\r\n--");
            while(!body_end) {
                idx += ngx_strlen((const char*)full_body->data+idx);
                if (idx < full_body->len-2) {
                    idx++;
                    body_end = (u_char*) ngx_strstr(full_body->data+idx, "\r\n--");
                } else
                    break;
            }

            if (!body_end) {
                yy_sec_waf_apply_mod_rule(r, NULL, uncommon_post_format, ctx);
            }

            if (!ngx_strncmp(body_end+4, boundary, boundary_len))
                break;
            else {
                idx += (u_char*)body_end - (full_body->data + idx) + 1;
                body_end = NULL;
            }
        }

        if (!body_end) {
            return NGX_ERROR;
        }

        if (filename.data) {
            nullbytes = ngx_yy_sec_waf_unescape(&filename);
            if (nullbytes > 0) {
                yy_sec_waf_apply_mod_rule(r, NULL, uncommon_hex_encoding, ctx);
            }

            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "[ysec_waf] checking filename [%V]", &filename);

            if (content_type.data) {
				ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
					"[ysec_waf] checking content_type [%V]", &content_type);

                if (!ngx_strnstr(filename.data, ".html", filename.len)
                    || !ngx_strnstr(filename.data, ".html", filename.len)) {
                    if (!ngx_strncmp(content_type.data, "text/html", content_type.len)) {
                        yy_sec_waf_apply_mod_rule(r, NULL, uncommon_filename, ctx);
                    }
                }
                else if (!ngx_strnstr(filename.data, ".php", filename.len)
                    || !ngx_strnstr(filename.data, ".jsp", filename.len)) {
                    if (!ngx_strncmp(content_type.data, "application/octet-stream", content_type.len)) {
                        yy_sec_waf_apply_mod_rule(r, NULL, uncommon_filename, ctx);
                    }
                }
            }

            yy_sec_waf_apply_mod_rule(r, &filename, special_file_charactor, ctx);
            
            yy_sec_waf_apply_mod_rule(r, &filename, uncommon_filename_postfix, ctx);

            idx += (u_char*)body_end - (full_body->data + idx);
        } else if (name.data) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "[ysec_waf] checking name [%V]", &name);

            idx += (u_char*)body_end - (full_body->data + idx);
        }

        if (!ngx_strncmp(body_end, "\r\n", ngx_strlen("\r\n")))
            idx += ngx_strlen("\r\n");
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_multipart Exit");

    return NGX_OK;
}

/*
** @description: This function is called to process the header of the request.
** @para: ngx_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @para: ngx_http_request_t *r
** @return: void.
*/

static void
ngx_http_yy_sec_waf_process_headers(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_loc_conf_t *cf, ngx_http_request_ctx_t *ctx)
{
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_headers Entry");

    ngx_list_part_t *part;
    ngx_table_elt_t *h;
    ngx_uint_t       i;

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; !ctx->matched; i++) {
        if (i >= part->nelts) {
            if (part->next == NULL) 
                break;

            part = part->next;
            h = part->elts;
            i = 0;
        }

        ngx_http_yy_sec_waf_process_basic_rules(r, &h[i].value, cf->header_rules, ctx);
	}

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_headers Exit");
}

/*
** @description: This function is called to process the uri of the request.
** @para: ngx_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @para: ngx_http_request_t *r
** @return: void.
*/

static void
ngx_http_yy_sec_waf_process_uri(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_loc_conf_t *cf, ngx_http_request_ctx_t *ctx)
{
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_uri Entry");

    ngx_str_t  tmp;

    tmp.len = r->uri.len;
    tmp.data = ngx_pcalloc(r->pool, tmp.len+1);

    if (tmp.data == NULL) {
        return;
    }

    (void)ngx_memcpy(tmp.data, r->uri.data, tmp.len);

    ngx_http_yy_sec_waf_process_basic_rules(r, &tmp, cf->uri_rules, ctx);

    ngx_pfree(r->pool, tmp.data);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_uri Exit");
}

/*
** @description: This function is called to process the args of the request.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_loc_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @return: void.
*/

static void
ngx_http_yy_sec_waf_process_args(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_loc_conf_t *cf, ngx_http_request_ctx_t *ctx)
{
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_args Entry");

    ngx_str_t  *tmp;

    tmp = ngx_palloc(r->pool, sizeof(ngx_str_t));
    if (tmp == NULL)
      return;

    tmp->len = r->args.len;
    tmp->data = ngx_pcalloc(r->pool, tmp->len+1);

    if (tmp->data == NULL) {
        return;
    }

    (void)ngx_memcpy(tmp->data, r->args.data, tmp->len);

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] decoded args:%V", tmp);

    ngx_http_yy_sec_waf_process_spliturl_rules(r, tmp, cf->args_rules, ctx);

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_args Exit");
}

/*
** @description: This function is called to process the body of the request.
** @para: ngx_http_request_t *r
** @para: ngx_http_yy_sec_waf_loc_conf_t *cf
** @para: ngx_http_request_ctx_t *ctx
** @return: NGX_OK or NGX_ERROR if failed.
*/

static ngx_int_t
ngx_http_yy_sec_waf_process_body(ngx_http_request_t *r,
    ngx_http_yy_sec_waf_loc_conf_t *cf, ngx_http_request_ctx_t *ctx)
{
	ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_body Entry");

    u_char      *src;
    ngx_chain_t *bb;
    ngx_str_t   *full_body;
	
    if (!r->request_body->bufs || !r->headers_in.content_type) {
        yy_sec_waf_apply_mod_rule(r, NULL, uncommon_content_type, ctx);
    }

    if (r->request_body->temp_file) {
        ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] post body is stored in temp_file.");
        return NGX_ERROR;
    }

    full_body = ngx_palloc(r->pool, sizeof(ngx_str_t));
    if (full_body == NULL) {
        return NGX_ERROR;
    }

    if (r->request_body->bufs->next == NULL) {
        full_body->len = (ngx_uint_t) (r->request_body->bufs->buf->last
            - r->request_body->bufs->buf->pos);

        full_body->data = ngx_pcalloc(r->pool, full_body->len+1);

        ngx_memcpy(full_body->data, r->request_body->bufs->buf->pos, full_body->len);
    } else {
        for (full_body->len = 0, bb = r->request_body->bufs; bb; bb = bb->next)
            full_body->len += bb->buf->last - bb->buf->pos;

        full_body->data = ngx_pcalloc(r->pool, full_body->len+1);

        if (full_body->data == NULL)
            return NGX_ERROR;

        src = full_body->data;

        for (bb = r->request_body->bufs; bb; bb = bb->next)
            full_body->data = ngx_cpymem(full_body->data, bb->buf->pos,
                bb->buf->last - bb->buf->pos);

        full_body->data = src;
    }

    //ngx_yy_sec_waf_unescape(full_body);

    if (!ngx_strncasecmp(r->headers_in.content_type->value.data,
        (u_char*)"multipart/form-data", ngx_strlen("multipart/form-data"))) {
        /* MULTIPART */
        ngx_http_yy_sec_waf_process_multipart(r, full_body, ctx);
    } else if (!ngx_strncasecmp(r->headers_in.content_type->value.data,
        (u_char*)"application/x-www-form-urlencoded", ngx_strlen("application/x-www-form-urlencoded"))) {
        /* X-WWW-FORM-URLENCODED */
        if (full_body->len > cf->max_post_args_len) {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] post method with more than %d args.",
                                                                      cf->max_post_args_len);
            return NGX_ERROR;
        }

        ngx_http_yy_sec_waf_process_spliturl_rules(r, full_body, cf->args_rules, ctx);
    }

    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_body Exit");

    return NGX_OK;
}

/*
** @description: This function is called to process the request.
** @para: ngx_http_request_t *r
** @return: NGX_OK or NGX_ERROR if failed.
*/

ngx_int_t
ngx_http_yy_sec_waf_process_request(ngx_http_request_t *r)
{
    ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_request Entry");

    ngx_http_yy_sec_waf_loc_conf_t *cf;
    ngx_http_request_ctx_t         *ctx;

	cf = ngx_http_get_module_loc_conf(r, ngx_http_yy_sec_waf_module);
    ctx = ngx_http_get_module_ctx(r, ngx_http_yy_sec_waf_module);

    if (cf == NULL || cf == NULL) {
        return NGX_ERROR;
    }


    if (cf->header_rules != NULL)
        ngx_http_yy_sec_waf_process_headers(r, cf, ctx);

    if (!ctx->matched && cf->uri_rules != NULL)
        ngx_http_yy_sec_waf_process_uri(r, cf, ctx);

    if (!ctx->matched && cf->args_rules != NULL)
        ngx_http_yy_sec_waf_process_args(r, cf, ctx);

    /* TODO: process body, need test case for this situation. */
    if ((r->method == NGX_HTTP_POST || r->method == NGX_HTTP_PUT)
        && r->request_body && !ctx->matched) {
        ngx_http_yy_sec_waf_process_body(r, cf, ctx);
    }

	ngx_log_debug(NGX_LOG_DEBUG_HTTP, r->connection->log, 0, "[ysec_waf] ngx_http_yy_sec_waf_process_request Exit");

    return NGX_OK;
}


