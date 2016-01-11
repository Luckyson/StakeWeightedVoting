/*
 * Copyright 2015 Follow My Vote, Inc.
 * This file is part of The Follow My Vote Stake-Weighted Voting Application ("SWV").
 *
 * SWV is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SWV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SWV.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BackendWrapper.hpp"
#include "PromiseConverter.hpp"
#include "wrappers/ContestGeneratorWrapper.hpp"
#include "wrappers/ContestCreationRequest.hpp"

#include <Promise.hpp>

#include <kj/debug.h>

#include <QDebug>
#include <QtQml>
#include <QVariant>

namespace swv {

BackendWrapper::BackendWrapper(Backend::Client backend, PromiseConverter& promiseConverter, QObject *parent)
    : QObject(parent),
      promiseConverter(promiseConverter),
      backend(kj::mv(backend))
{
}

ContestGeneratorWrapper* BackendWrapper::getFeedGenerator()
{
    return new ContestGeneratorWrapper(backend.getContestGeneratorRequest().send().getGenerator(), promiseConverter);
}

ContestGeneratorWrapper* BackendWrapper::getContestsByCreator(QString creator)
{
    auto request = backend.searchContestsRequest();
    auto filters = request.initFilters(1);
    filters[0].setType(Backend::Filter::Type::CONTEST_CREATOR);
    auto arguments = filters[0].initArguments(1);
    arguments.set(0, creator.toStdString());

    return new ContestGeneratorWrapper(request.send().getGenerator(), promiseConverter);
}

ContestGeneratorWrapper* BackendWrapper::getContestsByCoin(quint64 coinId)
{
    auto request = backend.searchContestsRequest();
    auto filters = request.initFilters(1);
    filters[0].setType(Backend::Filter::Type::CONTEST_COIN);
    auto arguments = filters[0].initArguments(1);
    arguments.set(0, std::to_string(coinId));

    return new ContestGeneratorWrapper(request.send().getGenerator(), promiseConverter);
}

ContestCreationRequest* BackendWrapper::getContestCreationRequest()
{
    // Fetch a new creator if we don't have one yet
    auto creator = [this] {
        KJ_IF_MAYBE(creator, contestCreator)
            return *creator;
        auto creator = backend.getContestCreatorRequest().send().getCreator();
        contestCreator = creator;
        return creator;
    }();

    auto pricesPromise = creator.getPriceScheduleRequest().send();
    auto pricesAndLimitsPromise = creator.getContestLimitsRequest().send().then(kj::mvCapture(kj::mv(pricesPromise),
        [](kj::Promise<capnp::Response<::ContestCreator::GetPriceScheduleResults>> pricesPromise,
           capnp::Response<::ContestCreator::GetContestLimitsResults> limits) {
        return pricesPromise.then(kj::mvCapture(limits,
                                                [] (decltype(limits) limits,
                                                    capnp::Response<::ContestCreator::GetPriceScheduleResults> prices) {
            return std::make_pair(kj::mv(prices), kj::mv(limits));
        }));
    }));

    return new ContestCreationRequest(creator.purchaseContestRequest());
}

} // namespace swv
