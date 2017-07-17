/*
 * Copyright (c) 2015 - 2016 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Contact: Jin Yoon <jinny.yoon@samsung.com>
 *          Geunsun Lee <gs86.lee@samsung.com>
 *          Eunyoung Lee <ey928.lee@samsung.com>
 *          Junkyu Han <junkyu.han@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <glib.h>
#include <Eina.h>

#include <iotcon.h>

#include "log.h"
#include "connectivity.h"

#define ULTRASONIC_RESOURCE_1_URI "/door/1"
#define ULTRASONIC_RESOURCE_2_URI "/door/2"
#define ULTRASONIC_RESOURCE_TYPE "org.tizen.door"

static bool _resource_created;
static void _request_resource_handler(iotcon_resource_h resource, iotcon_request_h request, void *user_data);

static int _send_response(iotcon_request_h request, iotcon_representation_h representation, iotcon_response_result_e result)
{
	int ret = -1;
	iotcon_response_h response;

	ret = iotcon_response_create(request, &response);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = iotcon_response_set_result(response, result);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_response_set_representation(response, representation);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_response_send(response);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	iotcon_response_destroy(response);

	return 0;

error:
	iotcon_response_destroy(response);
	return -1;
}

static void _destroy_representation(iotcon_representation_h representation)
{
	ret_if(!representation);
	iotcon_representation_destroy(representation);
}

static iotcon_representation_h _create_representation_with_attribute(iotcon_resource_h res, bool value)
{
	iotcon_attributes_h attributes = NULL;
	iotcon_representation_h representation = NULL;
	char *uri_path = NULL;
	int ret = -1;

	ret = iotcon_resource_get_uri_path(res, &uri_path);
	retv_if(IOTCON_ERROR_NONE != ret, NULL);

	ret = iotcon_representation_create(&representation);
	retv_if(IOTCON_ERROR_NONE != ret, NULL);

	ret = iotcon_attributes_create(&attributes);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_representation_set_uri_path(representation, uri_path);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_attributes_add_bool(attributes, "opened", value);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_representation_set_attributes(representation, attributes);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	iotcon_attributes_destroy(attributes);

	return representation;

error:
	if (attributes) iotcon_attributes_destroy(attributes);
	if (representation) iotcon_representation_destroy(representation);

	return NULL;
}

static int _handle_get_request(iotcon_resource_h res, iotcon_request_h request)
{
	iotcon_representation_h representation;
	int ret = -1;
	int value = 1;

	/* FIXME : We need to check the value of sensors */
	representation = _create_representation_with_attribute(res, (bool)value);
	retv_if(!representation, -1);

	ret = _send_response(request, representation, IOTCON_RESPONSE_OK);
	goto_if(0 != ret, error);

	_destroy_representation(representation);

	return 0;

error:
	_destroy_representation(representation);
	return -1;
}

static int _get_value_from_representation(iotcon_representation_h representation, bool *value)
{
	iotcon_attributes_h attributes;
	int ret = -1;

	ret = iotcon_representation_get_attributes(representation, &attributes);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = iotcon_attributes_get_bool(attributes, "opened", value);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	return 0;
}

static int _set_value_into_thing(iotcon_representation_h representation, bool value)
{
	/* FIXME : We need to set the value into the thing */
	return 0;
}

static int _handle_put_request(connectivity_resource_s *resource_info, iotcon_request_h request)
{
	iotcon_representation_h req_repr, resp_repr;
	int ret = -1;
	bool value = false;

	_D("PUT request");

	ret = iotcon_request_get_representation(request, &req_repr);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = _get_value_from_representation(req_repr, &value);
	retv_if(0 != ret, -1);

	ret = _set_value_into_thing(req_repr, value);
	retv_if(0 != ret, -1);

	resp_repr = _create_representation_with_attribute(resource_info->res, (bool)value);
	retv_if(NULL == resp_repr, -1);

	ret = _send_response(request, resp_repr, IOTCON_RESPONSE_OK);
	goto_if(0 != ret, error);

	ret = iotcon_resource_notify(resource_info->res, resp_repr, resource_info->observers, IOTCON_QOS_HIGH);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	_destroy_representation(resp_repr);

	return 0;

error:
	_destroy_representation(resp_repr);
	return -1;
}

int connectivity_notify(connectivity_resource_s *resource_info, int value)
{
	iotcon_representation_h representation;
	int ret = -1;

	retv_if(!resource_info, -1);
	retv_if(!resource_info->observers, -1);

	_D("Notify the value[%d]", value);

	representation = _create_representation_with_attribute(resource_info->res, (bool)value);
	retv_if(!representation, -1);

	ret = iotcon_resource_notify(resource_info->res, representation, resource_info->observers, IOTCON_QOS_HIGH);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	_destroy_representation(representation);

	return 0;
}

static int _handle_post_request(connectivity_resource_s *resource_info, iotcon_request_h request)
{
	iotcon_attributes_h resp_attributes = NULL;
	iotcon_representation_h resp_repr = NULL;
	connectivity_resource_s *new_resource_info = NULL;
	int ret = -1;

	_D("POST request");

	if (_resource_created) {
		_E("Resource(%s) is already created", ULTRASONIC_RESOURCE_2_URI);
		return -1;
	}

	new_resource_info = calloc(1, sizeof(connectivity_resource_s));
	retv_if(!new_resource_info, -1);

	ret = connectivity_set_resource(ULTRASONIC_RESOURCE_2_URI, ULTRASONIC_RESOURCE_TYPE, &new_resource_info);
	retv_if(0 != ret, -1);

	_resource_created = true;

	ret = iotcon_representation_create(&resp_repr);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = iotcon_attributes_create(&resp_attributes);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_attributes_add_str(resp_attributes, "createduripath", ULTRASONIC_RESOURCE_2_URI);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_representation_set_attributes(resp_repr, resp_attributes);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	iotcon_attributes_destroy(resp_attributes);

	ret = _send_response(request, resp_repr, IOTCON_RESPONSE_RESOURCE_CREATED);
	goto_if(0 != ret, error);

	iotcon_representation_destroy(resp_repr);

	return 0;

error:
	if (resp_attributes) iotcon_attributes_destroy(resp_attributes);
	iotcon_representation_destroy(resp_repr);
	return -1;
}

static int _handle_delete_request(iotcon_resource_h resource, iotcon_request_h request)
{
	int ret = -1;

	_D("DELETE request");

	ret = iotcon_resource_destroy(resource);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = _send_response(request, NULL, IOTCON_RESPONSE_RESOURCE_DELETED);
	retv_if(0 != ret, -1);

	return 0;
}

static bool _query_cb(const char *key, const char *value, void *user_data)
{
	_D("Key : [%s], Value : [%s]", key, value);

	return IOTCON_FUNC_CONTINUE;
}

static int _handle_query(iotcon_request_h request)
{
	iotcon_query_h query = NULL;
	int ret = -1;

	ret = iotcon_request_get_query(request, &query);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	if (query) iotcon_query_foreach(query, _query_cb, NULL);

	return 0;
}

static int _handle_request_by_crud_type(iotcon_request_h request, connectivity_resource_s *resource_info)
{
	iotcon_request_type_e type;
	int ret = -1;

	ret = iotcon_request_get_request_type(request, &type);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	switch (type) {
	case IOTCON_REQUEST_GET:
		ret = _handle_get_request(resource_info->res, request);
		break;
	case IOTCON_REQUEST_PUT:
		ret = _handle_put_request(resource_info, request);
		break;
	case IOTCON_REQUEST_POST:
		ret = _handle_post_request(resource_info, request);
		break;
	case IOTCON_REQUEST_DELETE:
		ret = _handle_delete_request(resource_info->res, request);
		break;
	default:
		_E("Cannot reach here");
		ret = -1;
		break;
	}
	retv_if(0 != ret, -1);

	return 0;
}

static int _handle_observer(iotcon_request_h request, iotcon_observers_h observers)
{
	iotcon_observe_type_e observe_type;
	int ret = -1;
	int observe_id = -1;

	ret = iotcon_request_get_observe_type(request, &observe_type);
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	if (IOTCON_OBSERVE_REGISTER == observe_type) {
		ret = iotcon_request_get_observe_id(request, &observe_id);
		retv_if(IOTCON_ERROR_NONE != ret, -1);

		ret = iotcon_observers_add(observers, observe_id);
		retv_if(IOTCON_ERROR_NONE != ret, -1);
	} else if (IOTCON_OBSERVE_DEREGISTER == observe_type) {
		ret = iotcon_request_get_observe_id(request, &observe_id);
		retv_if(IOTCON_ERROR_NONE != ret, -1);

		ret = iotcon_observers_remove(observers, observe_id);
		retv_if(IOTCON_ERROR_NONE != ret, -1);
	}

	return 0;
}

static void _request_resource_handler(iotcon_resource_h resource, iotcon_request_h request, void *user_data)
{
	connectivity_resource_s *resource_info = user_data;
	int ret = -1;
	char *host_address = NULL;

	ret_if(!request);

	ret = iotcon_request_get_host_address(request, &host_address);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	_D("Host address : %s", host_address);

	ret = _handle_query(request);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = _handle_request_by_crud_type(request, resource_info);
	goto_if(0 != ret, error);

	ret = _handle_observer(request, resource_info->observers);
	goto_if(0 != ret, error);

	return;

error:
	_send_response(request, NULL, IOTCON_RESPONSE_ERROR);
}

/* device_name : "iotcon-test-basic-server" */
int connectivity_init(const char *device_name)
{
	int ret = -1;

	ret = iotcon_initialize("/home/owner/apps_rw/org.tizen.position-finder-server/data/iotcon-test-svr-db-server.dat");
	retv_if(IOTCON_ERROR_NONE != ret, -1);

	ret = iotcon_set_device_name(device_name);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_start_presence(10);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	return 0;

error:
	iotcon_deinitialize();
	return -1;
}

int connectivity_fini(void)
{
	iotcon_deinitialize();
	return 0;
}

void connectivity_unset_resource(connectivity_resource_s *resource_info)
{
	ret_if(!resource_info);
	if (resource_info->observers) iotcon_observers_destroy(resource_info->observers);
	if (resource_info->res) iotcon_resource_destroy(resource_info->res);
	free(resource_info);
}

int connectivity_set_resource(const char *uri_path, const char *type, connectivity_resource_s **out_resource_info)
{
	iotcon_resource_types_h resource_types = NULL;
	iotcon_resource_interfaces_h ifaces = NULL;
	connectivity_resource_s *resource_info = NULL;
	uint8_t policies;
	int ret = -1;

	resource_info = calloc(1, sizeof(connectivity_resource_s));
	retv_if(!resource_info, -1);
	*out_resource_info = resource_info;

	ret = iotcon_resource_types_create(&resource_types);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_resource_types_add(resource_types, type);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_resource_interfaces_create(&ifaces);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_resource_interfaces_add(ifaces, IOTCON_INTERFACE_DEFAULT);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_resource_interfaces_add(ifaces, IOTCON_INTERFACE_BATCH);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	policies =
		IOTCON_RESOURCE_DISCOVERABLE |
		IOTCON_RESOURCE_OBSERVABLE |
		IOTCON_RESOURCE_SECURE;

	ret = iotcon_resource_create(uri_path,
			resource_types,
			ifaces,
			policies,
			_request_resource_handler,
			resource_info,
			&resource_info->res);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	ret = iotcon_observers_create(&resource_info->observers);
	goto_if(IOTCON_ERROR_NONE != ret, error);

	iotcon_resource_types_destroy(resource_types);
	iotcon_resource_interfaces_destroy(ifaces);

	return 0;

error:
	if (ifaces) iotcon_resource_interfaces_destroy(ifaces);
	if (resource_types) iotcon_resource_types_destroy(resource_types);
	if (resource_info->res) iotcon_resource_destroy(resource_info->res);
	if (resource_info) free(resource_info);

	return -1;
}
