// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/public/cpp/manifest.h"

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace service_manager {

const char kTestServiceName[] = "test_service";

const Manifest& GetPackagedService1Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_1")
                                                   .WithDisplayName("Service 1")
                                                   .Build()};
  return *manifest;
}

const Manifest& GetPackagedService2Manifest() {
  static base::NoDestructor<Manifest> manifest{ManifestBuilder()
                                                   .WithServiceName("service_2")
                                                   .WithDisplayName("Service 2")
                                                   .Build()};
  return *manifest;
}

const Manifest& GetManifest() {
  static base::NoDestructor<Manifest> manifest{
      ManifestBuilder()
          .WithServiceName(kTestServiceName)
          .WithDisplayName("The Test Service, Obviously")
          .WithOptions(
              ManifestOptionsBuilder()
                  .WithSandboxType("none")
                  .WithInstanceSharingPolicy(
                      Manifest::InstanceSharingPolicy::kSharedAcrossGroups)
                  .CanConnectToInstancesWithAnyId(true)
                  .CanConnectToInstancesInAnyGroup(true)
                  .CanRegisterOtherServiceInstances(false)
                  .Build())
          .ExposeCapability(
              "capability_1",
              Manifest::InterfaceList<mojom::Connector, mojom::PIDReceiver>())
          .ExposeCapability("capability_2",
                            Manifest::InterfaceList<mojom::Connector>())
          .RequireCapability("service_42", "computation")
          .RequireCapability("frobinator", "frobination")
          .ExposeInterfaceFilterCapability_Deprecated(
              "navigation:frame", "filter_capability_1",
              Manifest::InterfaceList<mojom::Connector>())
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "some_filter_capability")
          .RequireInterfaceFilterCapability_Deprecated(
              "browser", "navigation:frame", "another_filter_capability")
          .PackageService(GetPackagedService1Manifest())
          .PackageService(GetPackagedService2Manifest())
          .PreloadFile("file1_key",
                       base::FilePath(FILE_PATH_LITERAL("AUTOEXEC.BAT")))
          .PreloadFile("file2_key",
                       base::FilePath(FILE_PATH_LITERAL("CONFIG.SYS")))
          .PreloadFile("file3_key", base::FilePath(FILE_PATH_LITERAL(".vimrc")))
          .Build()};
  return *manifest;
}

TEST(ManifestTest, BasicBuilder) {
  const auto& manifest = GetManifest();
  EXPECT_EQ(kTestServiceName, manifest.service_name);
  EXPECT_EQ("none", manifest.options.sandbox_type);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_in_any_group);
  EXPECT_TRUE(manifest.options.can_connect_to_instances_with_any_id);
  EXPECT_FALSE(manifest.options.can_register_other_service_instances);
  EXPECT_EQ(Manifest::InstanceSharingPolicy::kSharedAcrossGroups,
            manifest.options.instance_sharing_policy);
  EXPECT_EQ(2u, manifest.exposed_capabilities.size());
  EXPECT_EQ(2u, manifest.required_capabilities.size());
  EXPECT_EQ(1u, manifest.exposed_interface_filter_capabilities.size());
  EXPECT_EQ(2u, manifest.required_interface_filter_capabilities.size());
  EXPECT_EQ(2u, manifest.packaged_services.size());
  EXPECT_EQ(manifest.packaged_services[0].service_name,
            GetPackagedService1Manifest().service_name);
  EXPECT_EQ(3u, manifest.preloaded_files.size());
}

TEST(ManifestTest, FromValueDeprecated) {
  constexpr const char kTestManifestJson[] = R"(
    {
      "name": "foo",
      "display_name": "bar",
      "sandbox_type": "utility",
      "services": [
        { "name": "packaged1" },
        { "name": "packaged2" }
      ],
      "options": {
        "can_connect_to_other_services_as_any_user": true,
        "can_connect_to_other_services_with_any_instance_name": true,
        "can_create_other_service_instances": true,
        "instance_sharing": "singleton"
      },
      "interface_provider_specs": {
        "service_manager:connector": {
          "provides": {
            "cap1": ["interface1", "interface2"],
            "cap2": ["interface3"],
            "cap3": []
          },
          "requires": {
            "a_service": ["cap3"],
            "another_service": ["cap4", "cap5"],
            "one_more_service": []
          }
        },
        "navigation:frame": {
          "provides": {
            "cap6": ["interface4"]
          },
          "requires": {
            "yet_another_service": ["cap7", "cap8"]
          }
        }
      }
    }
  )";
  const Manifest manifest{
      Manifest::FromValueDeprecated(base::JSONReader::Read(kTestManifestJson))};

  EXPECT_EQ("foo", manifest.service_name);
  EXPECT_EQ("bar", manifest.display_name.raw_string);

  EXPECT_EQ("utility", manifest.options.sandbox_type);
  EXPECT_EQ(Manifest::InstanceSharingPolicy::kSingleton,
            manifest.options.instance_sharing_policy);
  EXPECT_EQ(true, manifest.options.can_connect_to_instances_in_any_group);
  EXPECT_EQ(true, manifest.options.can_connect_to_instances_with_any_id);
  EXPECT_EQ(true, manifest.options.can_register_other_service_instances);

  const auto& exposed_capabilities = manifest.exposed_capabilities;
  ASSERT_EQ(3u, exposed_capabilities.size());
  EXPECT_EQ("cap1", exposed_capabilities[0].capability_name);
  EXPECT_THAT(exposed_capabilities[0].interface_names,
              ElementsAre("interface1", "interface2"));
  EXPECT_EQ("cap2", exposed_capabilities[1].capability_name);
  EXPECT_THAT(exposed_capabilities[1].interface_names,
              ElementsAre("interface3"));
  EXPECT_EQ("cap3", exposed_capabilities[2].capability_name);
  EXPECT_TRUE(exposed_capabilities[2].interface_names.empty());

  const auto& required_capabilities = manifest.required_capabilities;
  ASSERT_EQ(4u, required_capabilities.size());
  EXPECT_EQ("a_service", required_capabilities[0].service_name);
  EXPECT_EQ("cap3", required_capabilities[0].capability_name);
  EXPECT_EQ("another_service", required_capabilities[1].service_name);
  EXPECT_EQ("cap4", required_capabilities[1].capability_name);
  EXPECT_EQ("another_service", required_capabilities[2].service_name);
  EXPECT_EQ("cap5", required_capabilities[2].capability_name);
  EXPECT_EQ("one_more_service", required_capabilities[3].service_name);
  EXPECT_EQ("", required_capabilities[3].capability_name);

  const auto& exposed_filters = manifest.exposed_interface_filter_capabilities;
  ASSERT_EQ(1u, exposed_filters.size());
  EXPECT_EQ("navigation:frame", exposed_filters[0].filter_name);
  EXPECT_EQ("cap6", exposed_filters[0].capability_name);
  EXPECT_THAT(exposed_filters[0].interface_names, ElementsAre("interface4"));

  const auto& required_filters =
      manifest.required_interface_filter_capabilities;
  ASSERT_EQ(2u, required_filters.size());
  EXPECT_EQ("navigation:frame", required_filters[0].filter_name);
  EXPECT_EQ("yet_another_service", required_filters[0].service_name);
  EXPECT_EQ("cap7", required_filters[0].capability_name);
  EXPECT_EQ("navigation:frame", required_filters[1].filter_name);
  EXPECT_EQ("yet_another_service", required_filters[1].service_name);
  EXPECT_EQ("cap8", required_filters[1].capability_name);

  ASSERT_EQ(2u, manifest.packaged_services.size());
  EXPECT_EQ("packaged1", manifest.packaged_services[0].service_name);
  EXPECT_EQ("packaged2", manifest.packaged_services[1].service_name);
}

TEST(ManifestTest, Amend) {
  // Verify that everything is properly merged when amending potentially
  // overlapping capability metadata.
  Manifest manifest =
      ManifestBuilder()
          .ExposeCapability("cap1", {"interface1", "interface2"})
          .RequireCapability("service1", "cap2")
          .RequireCapability("service2", "cap3")
          .ExposeInterfaceFilterCapability_Deprecated(
              "filter1", "filtercap1", {"interface3", "interface4"})
          .RequireInterfaceFilterCapability_Deprecated("service3", "filter2",
                                                       "filtercap2")
          .Build();

  Manifest overlay = ManifestBuilder()
                         .ExposeCapability("cap1", {"xinterface1"})
                         .ExposeCapability("xcap1", {"xinterface2"})
                         .RequireCapability("xservice1", "xcap2")
                         .ExposeInterfaceFilterCapability_Deprecated(
                             "filter1", "filtercap1", {"xinterface3"})
                         .ExposeInterfaceFilterCapability_Deprecated(
                             "xfilter1", "xfiltercap1", {"xinterface4"})
                         .RequireInterfaceFilterCapability_Deprecated(
                             "xservice2", "xfilter2", "xfiltercap2")
                         .Build();

  manifest.Amend(std::move(overlay));

  const auto& exposed_capabilities = manifest.exposed_capabilities;
  ASSERT_EQ(2u, exposed_capabilities.size());
  EXPECT_EQ("cap1", exposed_capabilities[0].capability_name);
  EXPECT_THAT(exposed_capabilities[0].interface_names,
              ElementsAre("interface1", "interface2", "xinterface1"));

  const auto& required_capabilities = manifest.required_capabilities;
  ASSERT_EQ(3u, required_capabilities.size());
  EXPECT_EQ("service1", required_capabilities[0].service_name);
  EXPECT_EQ("cap2", required_capabilities[0].capability_name);
  EXPECT_EQ("service2", required_capabilities[1].service_name);
  EXPECT_EQ("cap3", required_capabilities[1].capability_name);
  EXPECT_EQ("xservice1", required_capabilities[2].service_name);
  EXPECT_EQ("xcap2", required_capabilities[2].capability_name);

  const auto& exposed_filters = manifest.exposed_interface_filter_capabilities;
  ASSERT_EQ(2u, exposed_filters.size());
  EXPECT_EQ("filter1", exposed_filters[0].filter_name);
  EXPECT_EQ("filtercap1", exposed_filters[0].capability_name);
  EXPECT_THAT(exposed_filters[0].interface_names,
              ElementsAre("interface3", "interface4", "xinterface3"));

  EXPECT_EQ("xfilter1", exposed_filters[1].filter_name);
  EXPECT_EQ("xfiltercap1", exposed_filters[1].capability_name);
  EXPECT_THAT(exposed_filters[1].interface_names, ElementsAre("xinterface4"));

  const auto& required_filters =
      manifest.required_interface_filter_capabilities;
  ASSERT_EQ(2u, required_filters.size());
  EXPECT_EQ("service3", required_filters[0].service_name);
  EXPECT_EQ("filter2", required_filters[0].filter_name);
  EXPECT_EQ("filtercap2", required_filters[0].capability_name);
  EXPECT_EQ("xservice2", required_filters[1].service_name);
  EXPECT_EQ("xfilter2", required_filters[1].filter_name);
  EXPECT_EQ("xfiltercap2", required_filters[1].capability_name);
}

}  // namespace service_manager
